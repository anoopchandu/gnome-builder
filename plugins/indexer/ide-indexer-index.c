#define G_LOG_DOMAIN "ide-indexer-index"

#include "ide-indexer-index.h"

struct _IdeIndexerIndex
{
  IdeObject   parent;

  GHashTable *indexes;
};

/* This class will store index of all directories and will have a map of
 * directory and Indexes(DzlFuzzyIndex & IdeSimpleTable)
 */

G_DEFINE_TYPE (IdeIndexerIndex, ide_indexer_index, IDE_TYPE_OBJECT)

static gboolean
indexer_file_equal (GFile *a, 
                    GFile *b)
{
  return g_file_equal (a, b);
}

static void
directory_index_free (DirectoryIndex *data)
{
  g_object_unref (data->names);
  g_object_unref (data->keys);
  g_slice_free (DirectoryIndex, data);
}

/* When index of a directory is loaded and again is called, this will
 * freshly load indexes again.
 */
DirectoryIndex*
ide_indexer_index_load_index (IdeIndexerIndex     *self,
                              GFile               *directory,
                              GCancellable        *cancellable,
                              GError             **error)
{
  DirectoryIndex *dir_index;
  g_autoptr(GFile) file1 = NULL, file2 = NULL;
  g_autoptr(DzlFuzzyIndex) names = NULL;
  g_autoptr(IdeSimpleTable) keys = NULL;

  g_return_val_if_fail (IDE_IS_INDEXER_INDEX (self), NULL);

  dir_index = g_hash_table_lookup (self->indexes, directory);

  if (dir_index == NULL)
    {
      names = dzl_fuzzy_index_new ();
      keys = ide_simple_table_new ();
    }
  else
    {
     names = g_object_ref (dir_index->names);
     keys = g_object_ref (dir_index->keys);
    }

  file1 = g_file_get_child (directory, "index1");
  if (!ide_simple_table_load_file (keys, file1, NULL, error))
    {
      g_hash_table_remove (self->indexes, directory);      
      return NULL;
    }

  file2 = g_file_get_child (directory, "index2");
  if (!dzl_fuzzy_index_load_file (names, file2, NULL, error))
    {
      g_hash_table_remove (self->indexes, directory);
      return NULL;
    }

  if (dir_index == NULL)
  {
    dir_index = g_slice_new (DirectoryIndex);
    dir_index->names = g_object_ref (names);
    dir_index->keys = g_object_ref (keys);
    g_hash_table_insert (self->indexes, g_file_dup (directory), dir_index);
  }

  return dir_index;
}

static void
ide_indexer_index_load_indexes_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data_ptr,
                                       GCancellable *cancellable)
{
  IdeIndexerIndex *self = source_object;
  GPtrArray *directories = (GPtrArray *)task_data_ptr;
  gint num_dirs;
  gboolean success = TRUE;
  GError *error = NULL;

  g_assert (IDE_IS_INDEXER_INDEX (self));
  g_assert (G_IS_TASK (task));

  num_dirs = directories->len;

  for (int i = 0; i < num_dirs; i++)
    {
      GFile *directory;

      directory = g_ptr_array_index (directories, i);
      if (ide_indexer_index_load_index (self, directory, cancellable, &error) == NULL)
        {
          success = FALSE;
          g_clear_error (&error);
        }
    }
  g_task_return_boolean (task, success);
}

void
ide_indexer_index_load_indexes_async (IdeIndexerIndex     *self,
                                      GPtrArray           *directories,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (IDE_IS_INDEXER_INDEX (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_ptr_array_ref (directories), (GDestroyNotify)g_ptr_array_unref);

  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task,
                             ide_indexer_index_load_indexes_worker);
}

gboolean
ide_indexer_index_load_indexes_finish (IdeIndexerIndex *self,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

gboolean
ide_indexer_index_fetch_declaration (IdeIndexerIndex *self, 
                                     const gchar     *key,
                                     guint           *fileid,
                                     guint           *line,
                                     guint           *coulmn)
{
  return FALSE;
}

void 
ide_indexer_index_fuzzy_search (IdeIndexerIndex *self,
                                const gchar     *key,
                                GList           *list)
{

}

static void
ide_indexer_index_dispose (GObject *object)
{
  IdeIndexerIndex *self = (IdeIndexerIndex *)object;

  g_hash_table_unref (self->indexes);
}

static void
ide_indexer_index_init (IdeIndexerIndex *self)
{
  self->indexes = g_hash_table_new_full (g_file_hash, (GEqualFunc) indexer_file_equal, 
                                         g_object_unref, (GDestroyNotify) directory_index_free);
}

static void
ide_indexer_index_class_init (IdeIndexerIndexClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->dispose = ide_indexer_index_dispose;
}

IdeIndexerIndex *
ide_indexer_index_new (IdeContext *context)
{
  return g_object_new (IDE_TYPE_INDEXER_INDEX,
                       "context", context,
                       NULL);
}