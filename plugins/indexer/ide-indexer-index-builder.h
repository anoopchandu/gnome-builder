/* ide-indexer-builder.h
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

#ifndef IDE_INDEXER_INDEX_BUILDER_H
#define IDE_INDEXER_INDEX_BUILDER_H

#include <ide.h>

#include "ide-indexer-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDEXER_INDEX_BUILDER (ide_indexer_index_builder_get_type ())

G_DECLARE_FINAL_TYPE (IdeIndexerIndexBuilder, ide_indexer_index_builder, IDE, INDEXER_INDEX_BUILDER, IdeObject)

IdeIndexerIndexBuilder *ide_indexer_index_builder_new          (IdeContext              *context,
                                                                GHashTable              *fileids,
                                                                IdeIndexerIndex         *index);
void                    ide_indexer_index_builder_build_async  (IdeIndexerIndexBuilder  *self,
                                                                GFile                   *directory,
                                                                gboolean                 recursive,
                                                                GCancellable            *cancellable,
                                                                GAsyncReadyCallback      callback,
                                                                gpointer                 user_data);
gboolean                ide_indexer_index_builder_build_finish (IdeIndexerIndexBuilder  *self,
                                                                GAsyncResult            *result,
                                                                GError                 **error);
G_END_DECLS

#endif /* IDE_INDEXER_INDEX_BUILDER_H */