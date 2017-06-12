/* ide-indexer-index-builder.c
 *
 * Copyright (C) 2017 Anoop Chandu <anoopchandu96@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-indexer-index-builder"

#include "ide-indexer-index-builder.h"

/* Indexing directory is done in 3 asynchronous steps:
 * Get indexing data (files to index and destination of index)
 * Index files and store index into destination
 * Loading index from files to memory
 */
 
struct _IdeIndexerIndexBuilder
{
	IdeObject         parent;

  IdeIndexerIndex  *index;
  GHashTable       *fileids;

  IdeAstIndexer    *ast_indexer;

  GQueue           *indexing_queue;
  GQueue           *loading_queue;
  guint             count;
};

typedef struct
{
  GPtrArray *files;
  GFile     *destination;
} IndexingData;

static gboolean ide_indexer_index_builder_get_indexing_data_finish (IdeIndexerIndexBuilder *self,
                                                                    GAsyncResult           *result,
                                                                    GError                **error);

G_DEFINE_TYPE (IdeIndexerIndexBuilder, ide_indexer_index_builder, IDE_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_INDEX,
  PROP_FILEIDS,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

const gchar *extensions[] = {".c",".h",".cc",".cp",".cxx",".cpp",".CPP",".c++",".C",
                             ".hh",".H",".hp",".hxx",".hpp",".HPP",".h++",".tcc", "\0"};

static void
indexing_data_free (IndexingData *data)
{
  g_ptr_array_unref (data->files);
  g_object_unref (data->destination);
  g_slice_free (IndexingData, data);
}

gboolean
ide_indexer_index_builder_build_finish (IdeIndexerIndexBuilder  *self,
                                        GAsyncResult            *result,
                                        GError                 **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
index_load_cb (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  IdeIndexerIndexBuilder *self;
  DzlFuzzyIndex *index = (DzlFuzzyIndex *)source_object;
  GError *error = NULL;
  GTask *main_task = (GTask *)user_data;

  g_assert (DZL_IS_FUZZY_INDEX (index));

  ide_indexer_index_load_file_finish (index, result, &error);

  self = g_task_get_source_object (main_task);
  self->count--;

  // g_print ("index load cb\n");

  if (g_queue_is_empty (self->indexing_queue) && g_queue_is_empty (self->loading_queue) && self->count==0)
    {
      // g_print ("Indexing complete\n");
      g_queue_free (self->indexing_queue);
      g_queue_free (self->loading_queue);
      g_task_return_boolean (main_task, TRUE);      
    }
  else if (!g_queue_is_empty (self->loading_queue))
    {
      g_autoptr(GFile) file;

      // g_print ("index_load_cb\n");

      file = g_queue_pop_head (self->loading_queue);
      ide_indexer_index_load_file_async (self->index, file,
                                         NULL, index_load_cb, main_task);
    }
}

static void
ast_indexer_index_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeIndexerIndexBuilder *self;
  IdeAstIndexer *indexer = (IdeAstIndexer *) source_object;
  GError *error = NULL;
  g_autoptr(GFile) file;
  GTask *main_task = (GTask *)user_data;

  g_assert (IDE_IS_AST_INDEXER (source_object));
  g_assert (G_IS_TASK (main_task));

  self = g_task_get_source_object (main_task);

  file = ide_ast_indexer_index_finish (indexer, result, &error);

  // g_print ("finished indexing, loading file\n");

  if (g_queue_is_empty (self->loading_queue))
    {
      ide_indexer_index_load_file_async (self->index, file,
                                         NULL, index_load_cb, main_task);
    }
  else
    {
      // g_print ("pushing loading index into queue\n");
      g_queue_push_tail (self->loading_queue, g_object_ref (file));
    }

  if (!g_queue_is_empty (self->indexing_queue))
    {
      // g_print ("+++++++++++\n");

      IndexingData *idata;

      idata = g_queue_pop_head (self->indexing_queue);
      ide_indexer_index_insert (self->index, idata->destination);
      ide_ast_indexer_index_async (self->ast_indexer,
                                   idata->files, self->fileids,
                                   idata->destination,
                                   NULL,
                                   ast_indexer_index_cb,
                                   user_data);
      indexing_data_free (idata);
    }
}

/* Callback for getting indexing data
 * This will take head of queue and index files in that element.
 */
static void
get_indexing_data_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeIndexerIndexBuilder *self = (IdeIndexerIndexBuilder *)object;
  GTask *main_task = (GTask *)user_data;
  GError *error = NULL;

  g_assert (IDE_IS_INDEXER_INDEX_BUILDER (self));
  g_assert (G_IS_TASK (main_task));

  ide_indexer_index_builder_get_indexing_data_finish (self, result, &error);

  if (g_queue_is_empty (self->indexing_queue))
    {
      g_task_return_boolean (main_task, TRUE); 
    }
  else
    {
      IndexingData *idata;

      idata = g_queue_pop_head (self->indexing_queue);

      ide_indexer_index_insert (self->index, idata->destination);
      ide_ast_indexer_index_async (self->ast_indexer,
                                   idata->files, self->fileids,
                                   idata->destination,
                                   NULL,
                                   ast_indexer_index_cb,
                                   user_data);
      indexing_data_free (idata);
    }
}

static void
ide_indexer_index_builder_get_indexing_data (IdeIndexerIndexBuilder *self,
                                             GFile                  *directory,
                                             GFile                  *destination,
                                             gboolean                recursive)
{
  GPtrArray *files = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr = NULL;
  GError *error = NULL;

  g_assert (IDE_IS_INDEXER_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (destination));

  /* These arrays has ownership of all files */
  files = g_ptr_array_new_with_free_func (g_object_unref);

  g_file_make_directory_with_parents (destination, NULL, &error);

  // g_print ("%s\n", g_file_get_uri (directory));

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          &error);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name;
      GFileType type;

      name = g_file_info_get_name (info);
      type = g_file_info_get_file_type (info);

      if (type == G_FILE_TYPE_DIRECTORY && recursive)
        {
          g_autoptr(GFile) sub_dir;
          g_autoptr(GFile) sub_dest;

          sub_dir = g_file_get_child (directory, name);
          sub_dest = g_file_get_child (destination, name);

          ide_indexer_index_builder_get_indexing_data (self, sub_dir, sub_dest, recursive);
        }
      else if (type == G_FILE_TYPE_REGULAR)
        {
          for (int i=0; extensions[i][0]; i++)
            {              
              if (g_str_has_suffix (name, extensions[i]))
                {
                  g_autoptr(GFile) file;

                  file = g_file_get_child (directory, name);

                  g_ptr_array_add (files, g_object_ref (file));
                  g_hash_table_insert (self->fileids,
                                       g_object_ref (file), 
                                       GUINT_TO_POINTER (g_hash_table_size (self->fileids)+1));
                }
            }
        }
    }

  if (files->len)
    {
      IndexingData *idata;

      idata = g_slice_new (IndexingData);
      idata->files = g_ptr_array_ref (files);
      idata->destination = g_file_get_child (destination, "index");
      g_queue_push_tail (self->indexing_queue, idata);
      self->count++;
    }

    g_ptr_array_unref (files);
}

static void
ide_indexer_index_builder_get_indexing_data_worker (GTask        *task,
                                                    gpointer      source_object,
                                                    gpointer      task_data_ptr,
                                                    GCancellable *cancellable)
{
  IdeIndexerIndexBuilder *self = (IdeIndexerIndexBuilder *)source_object;
  IdeContext *context;
  const gchar *project_id;
  GFile *workdir;
  g_autoptr(GFile) directory;
  g_autoptr(GFile) destination;
  g_autofree gchar *relative_path;
  g_autofree gchar *destination_path;
  gpointer *data = (gpointer *)task_data_ptr;

  self->ast_indexer = ide_context_get_ast_indexer (
                        ide_object_get_context (IDE_OBJECT (self)));

  directory = (GFile *)data[0];

  context = ide_object_get_context (IDE_OBJECT (self));
  project_id = ide_project_get_id (ide_context_get_project (context));
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));
  relative_path = g_file_get_relative_path (workdir, directory);
  destination_path = g_build_filename (g_get_user_cache_dir (),
                                       ide_get_program_name (),
                                       "index",
                                       project_id,
                                       relative_path,
                                       NULL);
  destination = g_file_new_for_path (destination_path);

  ide_indexer_index_builder_get_indexing_data (self,
                                               directory,
                                               destination,
                                               (GPOINTER_TO_UINT (data[1]))?TRUE:FALSE);

  g_task_return_boolean (task, TRUE);
}

static void
ide_indexer_index_builder_get_indexing_data_async (IdeIndexerIndexBuilder *self,
                                                   GFile                  *directory,
                                                   gboolean                recursive,
                                                   GCancellable           *cancellable,
                                                   GAsyncReadyCallback     callback,
                                                   gpointer                user_data)
{
  GTask *task;
  gpointer *data;

  g_assert (IDE_IS_INDEXER_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (directory));

  data = g_new (gpointer, 2);
  data[0] = g_file_dup (directory);
  data[1] = GUINT_TO_POINTER (recursive);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, data, g_free);

  g_task_run_in_thread (task, ide_indexer_index_builder_get_indexing_data_worker);
}

static gboolean
ide_indexer_index_builder_get_indexing_data_finish (IdeIndexerIndexBuilder *self,
                                                    GAsyncResult           *result,
                                                    GError                **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (task));

  return g_task_propagate_boolean (task, error);
}

void
ide_indexer_index_builder_build_async (IdeIndexerIndexBuilder  *self,
                                       GFile                   *directory,
                                       gboolean                 recursive,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback      callback,
                                       gpointer                 user_data)
{
  GTask *main_task;

  main_task = g_task_new (self, cancellable, callback, user_data);

  self->indexing_queue = g_queue_new ();
  self->loading_queue = g_queue_new ();

  ide_indexer_index_builder_get_indexing_data_async (self,
                                                     directory, recursive,
                                                     NULL,
                                                     get_indexing_data_cb,
                                                     main_task);
}

static void
ide_indexer_index_builder_set_property (GObject       *object,
                                        guint          prop_id,
                                        const GValue  *value,
                                        GParamSpec    *pspec)
{
  IdeIndexerIndexBuilder *self = (IdeIndexerIndexBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_get_object (value);
    case PROP_FILEIDS:
      self->fileids = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_indexer_index_builder_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeIndexerIndexBuilder *self = (IdeIndexerIndexBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_object (value, self->index);
      break;
    case PROP_FILEIDS:
      g_value_set_pointer (value, self->fileids);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_indexer_index_builder_init (IdeIndexerIndexBuilder *self)
{
}

static void
ide_indexer_index_builder_class_init (IdeIndexerIndexBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ide_indexer_index_builder_set_property;
  object_class->get_property = ide_indexer_index_builder_get_property;

  properties[PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "FuzzyIndex in which declaraions index will be stored.",
                         IDE_TYPE_INDEXER_INDEX,
                         G_PARAM_READWRITE);

  properties[PROP_FILEIDS] =
    g_param_spec_pointer ("fileids",
                          "File Ids",
                          "Dictionary of file - id pairs.",
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

IdeIndexerIndexBuilder *
ide_indexer_index_builder_new (IdeContext      *context,
                               GHashTable      *fileids,
                               IdeIndexerIndex *index)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_INDEXER_INDEX (index), NULL);

  return g_object_new (IDE_TYPE_INDEXER_INDEX_BUILDER,
                       "context", context,
                       "index", index,
                       "fileids", fileids,
                       NULL);
}
