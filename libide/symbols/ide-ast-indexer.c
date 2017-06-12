/* ide-ast-indexer.c
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

#define G_LOG_DOMAIN "ide-ast-indexer"

#include "ide-ast-indexer.h"

G_DEFINE_INTERFACE (IdeAstIndexer, ide_ast_indexer, IDE_TYPE_OBJECT)

static void
ide_ast_indexer_default_init (IdeAstIndexerInterface *iface)
{
}

void
ide_ast_indexer_index_async (IdeAstIndexer       *self,
                             GPtrArray           *files,
                             GHashTable          *fileids,
                             GFile               *destination,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  IdeAstIndexerInterface *iface;

  g_return_if_fail (IDE_IS_AST_INDEXER (self));

  iface = IDE_AST_INDEXER_GET_IFACE (self);

  if (iface->index_async != NULL)
    iface->index_async (self,
                        files,fileids,
                        destination,
                        cancellable, callback,
                        user_data);

}

gpointer 
ide_ast_indexer_index_finish (IdeAstIndexer   *self,
                              GAsyncResult    *result,
                              GError         **error)
{
  IdeAstIndexerInterface *iface;

  g_return_val_if_fail (IDE_IS_AST_INDEXER (self), NULL);

  iface  = IDE_AST_INDEXER_GET_IFACE (self);

  if (iface->index_finish != NULL)
    return iface->index_finish (self, result, error);
  else
    return NULL;
}
