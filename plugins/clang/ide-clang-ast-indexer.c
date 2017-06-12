/* ide-clang-ast-indexer.c
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

#define G_LOG_DOMAIN "ide-clang-ast-indexer"

#include "ide-clang-ast-indexer.h"
#include "ide-clang-service.h"


/* Task of this class is to index a list of files and store that index into
 * destination file. This is done in 2 parallel asynchronous processes:
 * Getting AST of file
 * Indexing AST of file
 *
 * After indexing all files then index is written into a file.
 */

struct _IdeClangAstIndexer
{
  IdeObject         parent_instance;

  IdeClangService  *service;
  IdeContext       *context;
};

typedef struct
{
  GPtrArray            *files;
  GQueue               *indexing_queue;
  GHashTable           *fileids;
  GFile                *destination;
  DzlFuzzyIndexBuilder *index_builder;
}TaskData;

typedef struct 
{
  IdeClangTranslationUnit *translation_unit;
  GFile                   *file;
}IndexingData;

static void ast_indexer_iface_init (IdeAstIndexerInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangAstIndexer, ide_clang_ast_indexer, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_AST_INDEXER,
                                               ast_indexer_iface_init))

static void
indexing_data_free (gpointer mem)
{
  IndexingData *data = (IndexingData *)mem;

  g_object_unref (data->file);
  g_slice_free (IndexingData, data);
}

static void
task_data_free (gpointer mem)
{
  TaskData *data = (TaskData *)mem;
  g_ptr_array_unref (data->files);
  g_object_unref (data->destination);
  g_object_unref (data->index_builder);
  g_hash_table_unref (data->fileids);
  g_queue_free_full (data->indexing_queue, indexing_data_free);

  g_slice_free (TaskData, data);
}

static void
write_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  DzlFuzzyIndexBuilder *index_builder = (DzlFuzzyIndexBuilder *)object;
  GTask *main_task = (GTask *)user_data;
  GError *error = NULL;
  TaskData *task_data;

  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (index_builder));
  g_assert (G_IS_TASK (main_task));

  dzl_fuzzy_index_builder_write_finish (index_builder, result, &error);
  g_print ("written \n");
  task_data = g_task_get_task_data (main_task);
  g_task_return_pointer (main_task, g_object_ref (task_data->destination),
                         g_object_unref);
}

static void
index_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeClangAstIndexer *self;
  IdeClangTranslationUnit *tu = (IdeClangTranslationUnit *)object;
  GTask *main_task = (GTask *)user_data;
  GQueue *indexing_queue;
  TaskData *task_data;
  GError *error = NULL;
  g_autoptr(IdeFile) ide_file;
  GPtrArray *files;
  IndexingData *idata;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (tu));
  g_assert (G_IS_TASK (main_task));

  ide_clang_translation_unit_index_finish (tu, result, &error);

  self = g_task_get_source_object (main_task);

  task_data = g_task_get_task_data (main_task);
  files = task_data->files;
  indexing_queue = task_data->indexing_queue;

  idata = g_queue_pop_head (indexing_queue);
  // g_print ("ide_clang_ast_indexer.., got index for %s\n", g_file_get_uri (idata->file));
  ide_file = ide_file_new (self->context, idata->file);
  ide_clang_service_evict_translation_unit (self->service, ide_file);
  indexing_data_free (idata);

  if (g_queue_is_empty (indexing_queue))
    {
      if (!files->len)
        {
          // g_print ("writing into file %s\n", g_file_get_uri (task_data->destination));
          dzl_fuzzy_index_builder_write_async (task_data->index_builder,
                                               task_data->destination,
                                               0,
                                               NULL,
                                               write_cb,
                                               user_data);
        }
    }
  else
    {
      idata = g_queue_peek_head (indexing_queue);

      ide_clang_translation_unit_index_async (idata->translation_unit,
                                              task_data->index_builder,
                                              g_hash_table_lookup (task_data->fileids, idata->file),
                                              NULL,
                                              index_cb,
                                              user_data);
    }
}

static void
get_translation_unit_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeClangAstIndexer *self;
  IdeClangService *service  = (IdeClangService *)object;
  IdeClangTranslationUnit *tu;
  GError *error;
  GFile *file;
  GTask *main_task = (GTask *)user_data;
  GPtrArray *files;
  TaskData *task_data;
  GQueue *indexing_queue;
  IndexingData *idata;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (G_IS_TASK (main_task));

  tu = ide_clang_service_get_translation_unit_finish (service, result, &error);

  task_data = (TaskData *)g_task_get_task_data (main_task);
  files = task_data->files;
  indexing_queue = task_data->indexing_queue;

  file = g_ptr_array_index (files, files->len-1);

  // g_print ("ide_clang_ast_indexer, got tu for %s\n", g_file_get_uri (file));

  idata = g_slice_new (IndexingData);
  idata->translation_unit = tu;
  idata->file = g_object_ref (file);

  if (g_queue_is_empty (indexing_queue))
    {
      g_queue_push_tail (indexing_queue, idata);
      // g_print ("ide_clang_ast_indexer.., getting index for %s\n", g_file_get_uri (idata->file));
      if (!IDE_IS_CLANG_TRANSLATION_UNIT (idata->translation_unit))
      {
        // g_print ("didn't get tu\n");
      }

      ide_clang_translation_unit_index_async (tu,
                                              task_data->index_builder,
                                              g_hash_table_lookup (task_data->fileids, idata->file),
                                              NULL,
                                              index_cb,
                                              user_data);
    }
  else
    {
      // g_print ("ide_clang_ast_indexer.., pushed for indexing %s\n", g_file_get_uri (idata->file));
      g_queue_push_tail (indexing_queue, idata);
    }

  g_ptr_array_remove_index (files, files->len-1);

  if (files->len)
  {
    file = g_ptr_array_index (files, files->len-1);

    self = g_task_get_source_object (main_task);

    // g_print ("getting tu for %s\n", g_file_get_uri (file));

    ide_clang_service_get_translation_unit_async (service, 
                                                  ide_file_new (self->context, file),
                                                  0,
                                                  NULL,
                                                  get_translation_unit_cb,
                                                  user_data);    
  }
}

/* This will get translation unit and index them parallelly */
void
ide_clang_ast_indexer_index_async (IdeAstIndexer       *indexer,
                                   GPtrArray           *files, 
                                   GHashTable          *fileids,
                                   GFile               *destination,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeClangAstIndexer *self = IDE_CLANG_AST_INDEXER (indexer);
  GFile *file;
  GTask *main_task;
  TaskData *task_data;

  g_return_if_fail (IDE_IS_CLANG_AST_INDEXER (self));

  if (!files->len) return;

  self->context = ide_object_get_context (IDE_OBJECT (self));
  self->service = ide_context_get_service_typed (self->context, IDE_TYPE_CLANG_SERVICE);

  task_data = g_slice_new (TaskData);
  task_data->files = g_ptr_array_ref (files);
  task_data->indexing_queue = g_queue_new ();
  task_data->fileids = g_hash_table_ref (fileids);
  task_data->destination = g_object_ref (destination);
  task_data->index_builder = dzl_fuzzy_index_builder_new ();

  main_task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (main_task, task_data, task_data_free);

  file = g_ptr_array_index (files, files->len-1);

  // g_print ("getting tu for %s\n", g_file_get_uri (file));

  ide_clang_service_get_translation_unit_async (self->service,
                                                ide_file_new (self->context, file),
                                                0,
                                                NULL,
                                                get_translation_unit_cb,
                                                main_task);
}

gpointer
ide_clang_ast_indexer_index_finish (IdeAstIndexer  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_ast_indexer_class_init (IdeClangAstIndexerClass *klass)
{
}

static void
ast_indexer_iface_init (IdeAstIndexerInterface *iface)
{
  iface->index_async = ide_clang_ast_indexer_index_async;
  iface->index_finish = ide_clang_ast_indexer_index_finish;
}

static void
ide_clang_ast_indexer_init (IdeClangAstIndexer *self)
{
}
