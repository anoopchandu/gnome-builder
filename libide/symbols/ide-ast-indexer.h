/* ide-ast-indexer.h
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

#ifndef IDE_AST_INDEXER_H
#define IDE_AST_INDEXER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_AST_INDEXER (ide_ast_indexer_get_type())

G_DECLARE_INTERFACE (IdeAstIndexer, ide_ast_indexer, IDE, AST_INDEXER, IdeObject)

struct _IdeAstIndexerInterface
{
    GTypeInterface parent_interface;

    void       (*index_async)   (IdeAstIndexer       *self,
                                 GPtrArray           *files,
                                 GHashTable          *fileids,
                                 GFile               *destination,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);
    gpointer   (*index_finish)  (IdeAstIndexer       *self,
                                 GAsyncResult        *result,
                                 GError             **error);
};

void     ide_ast_indexer_index_async   (IdeAstIndexer       *self,
                                        GPtrArray           *files,
                                        GHashTable          *fileids,
                                        GFile               *destination,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);
gpointer ide_ast_indexer_index_finish  (IdeAstIndexer       *self,
                                        GAsyncResult        *result,
                                        GError             **error);

G_END_DECLS

#endif /* IDE_AST_INDEXER */