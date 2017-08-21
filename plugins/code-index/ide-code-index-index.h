/* ide-code-index-index.h
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

#ifndef IDE_CODE_INDEX_INDEX_H
#define IDE_CODE_INDEX_INDEX_H

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_INDEX (ide_code_index_index_get_type())

G_DECLARE_FINAL_TYPE (IdeCodeIndexIndex, ide_code_index_index, IDE, CODE_INDEX_INDEX, IdeObject)

IdeCodeIndexIndex *ide_code_index_index_new               (IdeContext            *context);
/* This function will load index from directory if it is not modified. This fuction will only
 * load if all "files"(GPtrArray) and only those "files" are there in index.
 */
gboolean           ide_code_index_index_load_if_nmod      (IdeCodeIndexIndex     *self,
                                                           GFile                 *directory,
                                                           GPtrArray             *files,
                                                           GTimeVal               mod_time,
                                                           GCancellable          *cancellable,
                                                           GError               **error);
gboolean           ide_code_index_index_load              (IdeCodeIndexIndex     *self,
                                                           GFile                 *directory,
                                                           GCancellable          *cancellable,
                                                           GError               **error);
void               ide_code_index_index_populate_async    (IdeCodeIndexIndex     *self,
                                                           const gchar           *query,
                                                           gsize                  max_results,
                                                           GCancellable          *cancellable,
                                                           GAsyncReadyCallback    callback,
                                                           gpointer               user_data);
GPtrArray         *ide_code_index_index_populate_finish   (IdeCodeIndexIndex     *self,
                                                           GAsyncResult          *result,
                                                           GError               **error);
IdeSymbol         *ide_code_index_index_lookup_symbol     (IdeCodeIndexIndex     *self,
                                                           const gchar           *key);

G_END_DECLS

#endif /* IDE_CODE_INDEX_INDEX_H */
