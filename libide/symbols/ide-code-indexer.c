/* ide-code-indexer.c
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

#define G_LOG_DOMAIN "ide-code-indexer"

#include "ide-code-indexer.h"

G_DEFINE_INTERFACE (IdeCodeIndexer, ide_code_indexer, IDE_TYPE_OBJECT)

static GListModel *
ide_code_indexer_real_index_file (IdeCodeIndexer      *self,
                                  GFile               *file,
                                  gchar              **build_flags,
                                  GCancellable        *cancellable,
                                  GError             **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Indexing is not supported");
  return NULL;
}

void
ide_code_indexer_real_get_key_async (IdeCodeIndexer       *self,
                                     IdeSourceLocation    *location,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_code_indexer_real_get_key_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Get key is not supported");
}

gchar *
ide_code_indexer_real_get_key_finish (IdeCodeIndexer  *self,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (task, error);
}

static void
ide_code_indexer_default_init (IdeCodeIndexerInterface *iface)
{
  iface->index_file = ide_code_indexer_real_index_file;
  iface->get_key_async = ide_code_indexer_real_get_key_async;
  iface->get_key_finish = ide_code_indexer_real_get_key_finish;
}

/**
 * ide_code_indexer_index_file:
 * @self: A #IdeCodeIndexer instance.
 * @file: Source file to index.
 * @build_flags: array of build flags to parse @file.
 * @cancellable: (allow-none): A #GCancellable.
 * @error: A #GError.
 *
 * This function will take index source file and create an array
 * of symbols in @file.
 *
 * Returns: (transfer full) : A #GListModel contains list
 *    of #IdeCodeIndexEntry.
 */
GListModel *
ide_code_indexer_index_file (IdeCodeIndexer      *self,
                             GFile               *file,
                             gchar              **build_flags,
                             GCancellable        *cancellable,
                             GError             **error)
{
  IdeCodeIndexerInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEXER (self), NULL);

  iface = IDE_CODE_INDEXER_GET_IFACE (self);

  return iface->index_file (self, file, build_flags, cancellable, error);
}

/**
 * ide_code_indexer_get_key_async:
 * @self: A #IdeCodeIndexer instance.
 * @location: Source location of refernece.
 * @cancellable: (allow-none): A #GCancellable.
 * @callback: A callback to execute upon indexing.
 * @user_data: User data to pass to @callback.
 *
 * This function will get key of reference located at #IdeSoureLocation.
 */
void
ide_code_indexer_get_key_async (IdeCodeIndexer       *self,
                                IdeSourceLocation    *location,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
  IdeCodeIndexerInterface *iface;

  g_return_if_fail (IDE_IS_CODE_INDEXER (self));

  iface = IDE_CODE_INDEXER_GET_IFACE (self);

  iface->get_key_async (self, location, cancellable, callback, user_data);
}

/**
 * ide_code_indexer_get_key_finish:
 *
 * Returns key for declaration of reference at a location.
 *
 * Returns: (transfer full) : A string which contains key.
 */
gchar *
ide_code_indexer_get_key_finish (IdeCodeIndexer       *self,
                                 GAsyncResult         *result,
                                 GError              **error)
{
  IdeCodeIndexerInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEXER (self), NULL);

  iface  = IDE_CODE_INDEXER_GET_IFACE (self);

  return iface->get_key_finish (self, result, error);
}
