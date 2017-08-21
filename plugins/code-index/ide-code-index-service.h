/* ide-code-index-service.h
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

#ifndef IDE_CODE_INDEX_SERVICE_H
#define IDE_CODE_INDEX_SERVICE_H

#include <ide.h>

#include "ide-code-index-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_SERVICE  (ide_code_index_service_get_type())

G_DECLARE_FINAL_TYPE (IdeCodeIndexService, ide_code_index_service, IDE, CODE_INDEX_SERVICE, IdeObject)

IdeCodeIndexIndex *ide_code_index_service_get_index                 (IdeCodeIndexService *self);
IdeCodeIndexer    *ide_code_index_service_get_code_indexer          (IdeCodeIndexService *self,
                                                                     const gchar         *file_name);

G_END_DECLS

#endif /* IDE_CODE_INDEX_SERVICE_H */
