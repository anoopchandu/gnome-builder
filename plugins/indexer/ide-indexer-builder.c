#define G_LOG_DOMAIN "ide-indexer-builder"

#include "ide-indexer-builder.h"

/* Building index is done in 3 subtasks.
 * 1.Getting changes - This will get sets of files which needs to be indexed. Each set of 
 *                     files will have some destination to store index of them. When this finishes
 *                     it gives to task 2 a queue of set of files to index.    
 * 2.Indexing - This will get build flags for all files and give each set of files with build
 *              flags to subprocess to index. After this finished it gives to task 3 queue of files
 *              to load.
 * 3.Loading- This will load all files which are indexed.
 * 
 * sub task 1 -> changes_queue -> sub task 2 -> loading_queue -> sub task 3 
 */
 
struct _IdeIndexerBuilder
{
	IdeObject         parent;

  IdeIndexerIndex  *index;
};

typedef struct
{
  GPtrArray *files;
  GFile     *destination;
} ChangesQueueData;

typedef struct
{
  GPtrArray         *changes_queue;
  GPtrArray         *loading_queue;
  IdeSubprocess     *subprocess;
  GPtrArray         *files;
  GString           *indexer_input;
} IndexingData;

typedef struct
{
  GFile    *directory;
  gboolean  recursive;
} SubTask1Data;

const gchar *extensions[] = {".c",".h",".cc",".cp",".cxx",".cpp",".CPP",".c++",".C",
                            ".hh",".H",".hp",".hxx",".hpp",".HPP",".h++",".tcc", "\0"};

G_DEFINE_TYPE (IdeIndexerBuilder, ide_indexer_builder, IDE_TYPE_OBJECT)

static GPtrArray* ide_indexer_builder_get_changes_finish (IdeIndexerBuilder *self,
                                                          GAsyncResult      *result,
                                                          GError           **error);
static void       ide_indexer_builder_index              (IdeIndexerBuilder *self,
                                                          GTask             *main_task);

enum
{
  PROP_0,
  PROP_INDEX,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
changes_queue_data_free (ChangesQueueData *data)
{
  g_ptr_array_unref (data->files);
  g_object_unref (data->destination);
  g_slice_free (ChangesQueueData, data);
}

static void
sub_task1_data_free (SubTask1Data *data)
{
  g_object_unref (data->directory);
  g_slice_free (SubTask1Data, data);
}

static void
indexing_data_free (IndexingData *data)
{
  g_ptr_array_unref (data->changes_queue);
  g_ptr_array_unref (data->loading_queue);
  g_slice_free (IndexingData, data);
}

/* This is called after all 3 tasks are finished. This will complete the task of indexing and loading */
static void
index_loaded_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  IdeIndexerIndex *index = (IdeIndexerIndex *)source_object;
  GTask *main_task = (GTask *)user_data;
  GError *error = NULL;

  g_assert (IDE_IS_INDEXER_INDEX (index));
  g_assert (G_IS_TASK (main_task));

  ide_indexer_index_load_indexes_finish (index, result, &error);
  g_debug ("sub task 3 - loading finished");

  g_task_return_boolean (main_task, TRUE);
}

/* Sub Task 3 - Loading indexes.*/

/* This function is callback when subprocess finishes indexing, i.e sub task 2 completed.
 * This will load all indexes.
 */
static void
indexing_finished_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeIndexerBuilder *self;
  IdeSubprocess *subprocess = (IdeSubprocess *)source_object;
  GTask *main_task = (GTask *)user_data;
  GError *error = NULL;
  GPtrArray *loading_queue;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_TASK (main_task));

  self = g_task_get_source_object (main_task);
  loading_queue = g_task_get_task_data (main_task);

  ide_subprocess_wait_finish (subprocess, result, &error);
  g_debug ("subtask 2 finished, indexing finished");
  g_debug ("starting sub task 3 loading indexes");

  ide_indexer_index_load_indexes_async (self->index,
                                        loading_queue,
                                        NULL,
                                        index_loaded_cb,
                                        main_task);
}

/* Sub Task 2 - Indexing files. This is a loop inside loop. Outer loop will taken each set of 
 * files from changes queue and give that to inner loop. Inner loop will take each file from 
 * files array, get build flags and put that into subprocess.
 */

/* This function puts retrieved build flags into subprocess input string. If still there are files
 * to build flags this will call get_build_flags_async again. This is from ide-clang-serivce.c.
 */
static void
get_build_flags_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeIndexerBuilder *self;
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  gchar **argv = NULL;
  GError *error = NULL;
  GTask *main_task = (GTask *)user_data;
  IndexingData *idata;
  GPtrArray *files;
  GString *indexer_input;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_TASK (main_task));

  idata = g_task_get_task_data (main_task);
  self = g_task_get_source_object (main_task);

  argv = ide_build_system_get_build_flags_finish (build_system, result, &error);

  files = idata->files;
  indexer_input = idata->indexer_input;

  if (!argv || !argv[0])
    {
      IdeConfigurationManager *manager;
      IdeConfiguration *config;
      IdeContext *context;
      const gchar *cflags;
      const gchar *cxxflags;

      g_clear_pointer (&argv, g_strfreev);

      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_message ("%s", error->message);
      g_clear_error (&error);

      context = ide_object_get_context (IDE_OBJECT (build_system));
      manager = ide_context_get_configuration_manager (context);
      config = ide_configuration_manager_get_current (manager);
      cflags = ide_configuration_getenv (config, "CFLAGS");
      cxxflags = ide_configuration_getenv (config, "CXXFLAGS");

      if (cflags && *cflags)
        g_shell_parse_argv (cflags, NULL, &argv, NULL);

      if (cxxflags && (!argv || !*argv))
        g_shell_parse_argv (cxxflags, NULL, &argv, NULL);

      if (argv == NULL)
        argv = g_new0 (gchar*, 1);
    }

  for (int i = 0; argv[i] != NULL; i++)
    g_string_append_printf (indexer_input, "%s ", argv[i]);
  g_string_append_printf (indexer_input, "%s\n", "-I/usr/lib/llvm-4.0/bin/../lib/clang/4.0.0/include");

  if (files->len) /* If still there are files to get build flags, get those*/
    {
      GFile *file;
      g_autofree gchar *file_name;

      file = g_ptr_array_index (files, files->len - 1);

      file_name = g_file_get_path (file);
      g_string_append_printf (indexer_input, "%s ", file_name);

      ide_build_system_get_build_flags_async (build_system,
                                              ide_file_new (ide_object_get_context (IDE_OBJECT (self)), g_object_ref (file)),
                                              NULL,
                                              get_build_flags_cb,
                                              main_task);
      g_ptr_array_remove_index (files, files->len - 1);
    }
  else
    {
      GOutputStream *indexer_pipe;
      gsize num_bytes;

      indexer_pipe = (GOutputStream *)ide_subprocess_get_stdin_pipe (idata->subprocess);

      g_output_stream_write_all (indexer_pipe, indexer_input->str, indexer_input->len, &num_bytes, NULL, &error);

      g_clear_pointer (&idata->files, g_ptr_array_unref);
      g_string_free (idata->indexer_input, TRUE);
      idata->indexer_input = NULL;
      g_debug ("started indexing 1");
      ide_indexer_builder_index (self, main_task);
    }
}

/* This function takes each files set from changes_queue and give that to subprocess */
static void
ide_indexer_builder_index (IdeIndexerBuilder *self,
                           GTask             *main_task)
{
  IndexingData *idata;

  g_assert (IDE_IS_INDEXER_BUILDER (self));
  g_assert (G_IS_TASK (main_task));

  idata = g_task_get_task_data (main_task);

  if (idata->changes_queue->len == 0)
    {
      gsize num_bytes;
      GError *error = NULL;
      GPtrArray *loading_queue;
      GOutputStream *output;

      output = ide_subprocess_get_stdin_pipe (idata->subprocess);

      g_output_stream_printf (output, &num_bytes, NULL, &error, "0\n");
      g_output_stream_close (output, NULL, &error);


      loading_queue = g_ptr_array_ref (idata->loading_queue);
      g_task_set_task_data (main_task, loading_queue, (GDestroyNotify)g_ptr_array_unref);

      ide_subprocess_wait_async (idata->subprocess, NULL, indexing_finished_cb, main_task);
    }
  else
    { /* get set of files from changes_queue, and get build flags for those */
      ChangesQueueData *cdata;
      GPtrArray *files;
      GFile *destination, *file;
      GString *indexer_input;
      g_autofree gchar *dest_name, *file_name;
      IdeContext *context;

      cdata = g_ptr_array_index (idata->changes_queue, idata->changes_queue->len - 1);

      files = cdata->files;
      indexer_input = g_string_new (NULL);
      destination = cdata->destination;

      dest_name = g_file_get_path (destination);
      g_string_append_printf (indexer_input, "%d\n%s\n", files->len, dest_name);

      file = g_ptr_array_index (files, files->len - 1);

      file_name = g_file_get_path (file);
      g_string_append_printf (indexer_input, "%s ", file_name);

      idata->indexer_input = indexer_input;
      idata->files = g_ptr_array_ref (files);
      g_ptr_array_add (idata->loading_queue, g_object_ref (destination));

      context = ide_object_get_context (IDE_OBJECT (self));
      ide_build_system_get_build_flags_async (ide_context_get_build_system (context),
                                              ide_file_new (context, g_object_ref (file)),
                                              NULL,
                                              get_build_flags_cb,
                                              main_task);
      g_ptr_array_remove_index (files, files->len - 1);
      g_ptr_array_remove_index (idata->changes_queue, idata->changes_queue->len - 1);
    }
}

/* This function is starting of sub task 2. This is called when all changes in the directory are
 * retrieved and put into changes_queue. This will push each set of files into subprocess to index.
 */
static void
get_changes_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IdeIndexerBuilder *self = (IdeIndexerBuilder *)object;
  GTask *main_task = (GTask *)user_data;
  GError *error = NULL;
  g_autoptr(GPtrArray) changes_queue;

  g_assert (IDE_IS_INDEXER_BUILDER (self));
  g_assert (G_IS_TASK (main_task));

  changes_queue = ide_indexer_builder_get_changes_finish (self, result, &error);
  g_debug ("Sub task 1 finished, got file changes");

  if (changes_queue->len == 0)
    {
      g_debug ("No changes are there, completing task");
      g_task_return_boolean (main_task, TRUE);
      g_object_unref (main_task);
    }
  else
    {
      g_autoptr(IdeSubprocessLauncher) launcher;
      IdeSubprocess *subprocess;
      IndexingData *idata;

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_SILENCE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "indexer");
      subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);

      if (subprocess == NULL)
        {
          g_debug ("%s", error->message);
          g_task_return_boolean (main_task, FALSE);
          g_object_unref (main_task);
          return;
        }

      idata = g_slice_new0 (IndexingData);
      idata->changes_queue = g_ptr_array_ref (changes_queue);
      idata->subprocess = subprocess;
      idata->loading_queue = g_ptr_array_new_full (changes_queue->len, g_object_unref);
      g_task_set_task_data (main_task, idata, (GDestroyNotify)indexing_data_free);

      g_debug ("Starting Sub task 2 - indexing %d sets", changes_queue->len);
      ide_indexer_builder_index (self, main_task);
    }
}

/* Sub Task 1 - getting changes */

/* This will retive all directories which needs to be reindexed */
static void
ide_indexer_builder_get_changes (IdeIndexerBuilder *self,
                                 GFile             *directory,
                                 GFile             *destination,
                                 gboolean           recursive,
                                 GPtrArray         *changes_queue)
{
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr = NULL;
  GError *error = NULL;
  gboolean reindex = FALSE;
  g_autoptr(GFile) dest_file = NULL;
  DirectoryIndex *dir_index;
  DzlFuzzyIndex *fzy_index = NULL;
  guint num_files = 0;
  GTimeVal indexing_time = {0,0};

  g_assert (IDE_IS_INDEXER_BUILDER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (destination));

  // g_debug ("traversing %s", g_file_get_uri (directory));

  dir_index = ide_indexer_index_load_index (self->index, destination, NULL, &error);
  if (dir_index != NULL)
    fzy_index = dir_index->names;

  g_clear_error (&error);

  dest_file = g_file_get_child (destination, "index2");

  if (fzy_index == NULL)
    {
      reindex = TRUE;
      // g_debug ("%s not found", g_file_get_uri (dest_file));
      g_file_make_directory_with_parents (destination, NULL, &error);
    }
  else
    {
      num_files = dzl_fuzzy_index_get_metadata_uint32 (fzy_index, "NumFiles");
      // g_debug ("%s found %d files", g_file_get_uri (dest_file), num_files);
      infoptr = g_file_query_info (dest_file,
                                   G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL,
                                   &error);
      if (error != NULL)
        g_debug ("is null %s", error->message);
      g_file_info_get_modification_time (infoptr, &indexing_time);
    }
  g_clear_error (&error);

  files = g_ptr_array_new_with_free_func (g_object_unref); 

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          &error);
  g_clear_error (&error);

  while ((infoptr = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL)
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name;
      GFileType type;

      name = g_file_info_get_name (info);
      type = g_file_info_get_file_type (info);

      if (type == G_FILE_TYPE_DIRECTORY && recursive)
        {
          g_autoptr(GFile) sub_dir, sub_dest;

          sub_dir = g_file_get_child (directory, name);
          sub_dest = g_file_get_child (destination, name);

          ide_indexer_builder_get_changes (self, sub_dir, sub_dest, recursive, changes_queue);
        }
      else if (type == G_FILE_TYPE_REGULAR)
        {
          for (int i=0; extensions[i][0]; i++)
            {
              if (g_str_has_suffix (name, extensions[i]))
                {
                  GTimeVal modtime;
                  
                  // g_debug ("%s", name);
                  g_file_info_get_modification_time (info, &modtime);

                  if (fzy_index != NULL && !dzl_fuzzy_index_get_metadata_uint32 (fzy_index, name))
                      reindex = TRUE;

                  if ((modtime.tv_sec > indexing_time.tv_sec) ||
                     ((modtime.tv_sec == indexing_time.tv_sec) && (modtime.tv_usec > indexing_time.tv_usec)))
                      reindex = TRUE;

                  g_ptr_array_add (files, g_file_get_child (directory, name));
                  break;
                }
            }
        }
    }
  g_clear_error (&error);

  if (num_files != files->len)
    reindex = TRUE;

  if (reindex && files->len)
    {
      ChangesQueueData *cdata;

      cdata = g_slice_new (ChangesQueueData);
      cdata->files = g_ptr_array_ref (files);
      cdata->destination = g_object_ref (destination);
      g_ptr_array_add (changes_queue, cdata);
    }
}

static void
ide_indexer_builder_get_changes_worker (GTask        *sub_task1,
                                        gpointer      source_object,
                                        gpointer      task_data_ptr,
                                        GCancellable *cancellable)
{
  IdeIndexerBuilder *self = (IdeIndexerBuilder *)source_object;
  IdeContext *context;
  const gchar *project_id;
  GFile *workdir;
  g_autoptr(GFile) destination = NULL;
  g_autofree gchar *relative_path, *destination_path;
  SubTask1Data *st1_data = task_data_ptr;
  GPtrArray *changes_queue = NULL;

  g_assert (IDE_IS_INDEXER_BUILDER (self));
  g_assert (G_IS_TASK (sub_task1));

  context = ide_object_get_context (IDE_OBJECT (self));
  project_id = ide_project_get_id (ide_context_get_project (context));
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));
  relative_path = g_file_get_relative_path (workdir, st1_data->directory);
  destination_path = g_build_filename (g_get_user_cache_dir (),
                                       ide_get_program_name (),
                                       "index",
                                       project_id,
                                       relative_path,
                                       NULL);
  destination = g_file_new_for_path (destination_path);

  changes_queue = g_ptr_array_new_with_free_func ((GDestroyNotify)changes_queue_data_free);
  ide_indexer_builder_get_changes (self, st1_data->directory, destination, st1_data->recursive, changes_queue);

  g_task_return_pointer (sub_task1, changes_queue, (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_indexer_builder_get_changes_async (IdeIndexerBuilder   *self,
                                       GFile               *directory,
                                       gboolean             recursive,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) sub_task1;
  SubTask1Data *st1_data;

  g_assert (IDE_IS_INDEXER_BUILDER (self));
  g_assert (G_IS_FILE (directory));

  st1_data = g_slice_new (SubTask1Data);
  st1_data->directory = g_object_ref (directory);
  st1_data->recursive = recursive;

  sub_task1 = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (sub_task1, st1_data, (GDestroyNotify)sub_task1_data_free);

  g_debug ("Sub task 1 started - getting changes");
  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, sub_task1, 
                             ide_indexer_builder_get_changes_worker);
}

static GPtrArray*
ide_indexer_builder_get_changes_finish (IdeIndexerBuilder *self,
                                        GAsyncResult      *result,
                                        GError           **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

/* Main Task - Building Index */

void
ide_indexer_builder_build_async (IdeIndexerBuilder   *self,
                                 GFile               *directory,
                                 gboolean             recursive,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GTask *main_task;

  g_return_if_fail (IDE_IS_INDEXER_BUILDER (self));

  main_task = g_task_new (self, cancellable, callback, user_data);

  ide_indexer_builder_get_changes_async (self,
                                         directory,
                                         recursive,
                                         NULL,
                                         get_changes_cb,
                                         main_task);
}

/* Returns success or failure of index building task.*/
gboolean
ide_indexer_builder_build_finish (IdeIndexerBuilder  *self,
                                  GAsyncResult            *result,
                                  GError                 **error)
{
  GTask *main_task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (main_task), FALSE);

  return g_task_propagate_boolean (main_task, error);
}

static void
ide_indexer_builder_set_property (GObject       *object,
                                  guint          prop_id,
                                  const GValue  *value,
                                  GParamSpec    *pspec)
{
  IdeIndexerBuilder *self = (IdeIndexerBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_indexer_builder_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeIndexerBuilder *self = (IdeIndexerBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_object (value, self->index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_indexer_builder_init (IdeIndexerBuilder *self)
{
}

static void
ide_indexer_builder_class_init (IdeIndexerBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ide_indexer_builder_set_property;
  object_class->get_property = ide_indexer_builder_get_property;

  properties[PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "FuzzyIndex in which declaraions index will be stored.",
                         IDE_TYPE_INDEXER_INDEX,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

IdeIndexerBuilder *
ide_indexer_builder_new (IdeContext      *context,
                         IdeIndexerIndex *index)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_INDEXER_INDEX (index), NULL);

  return g_object_new (IDE_TYPE_INDEXER_BUILDER,
                       "context", context,
                       "index", index,
                       NULL);
}
