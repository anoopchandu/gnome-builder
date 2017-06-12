/* ide-indexer-index.h
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

#ifndef IDE_INDEXER_INDEX_H
#define IDE_INDEXER_INDEX_H

#include <ide.h>

G_BEGIN_DECLS

/* This will have a map of index file and its corresponding DzlFuzzyIndex*/
#define IDE_TYPE_INDEXER_INDEX (ide_indexer_index_get_type())
G_DECLARE_FINAL_TYPE (IdeIndexerIndex, ide_indexer_index, IDE, INDEXER_INDEX, IdeObject)

/* Creates a new DzlFuzzyIndex object for a file if not exists. Returns whether
 * a DzlFuzzyIndex object already exisits for a file.
 */
gboolean         ide_indexer_index_insert            (IdeIndexerIndex     *self,
                                                      GFile               *file);
void             ide_indexer_index_load_file_async   (IdeIndexerIndex     *self,
                                                      GFile               *file,
                                                      GCancellable        *cancellable,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             user_data);
gboolean         ide_indexer_index_load_file_finish  (DzlFuzzyIndex       *self,
                                                      GAsyncResult        *result,
                                                      GError             **error);
gboolean         ide_indexer_index_fetch_declaration (IdeIndexerIndex     *self, 
                                                      const gchar         *key,
                                                      guint               *fileid,
                                                      guint               *line,
                                                      guint               *coulmn);
void             ide_indexer_index_fuzzy_search      (IdeIndexerIndex     *self,
                                                      const gchar         *key,
                                                      GList               *list);
IdeIndexerIndex *ide_indexer_index_new               (GHashTable          *fileids);
G_END_DECLS

#endif /* IDE_INDEXER_INDEX_H */
