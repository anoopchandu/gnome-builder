/* ide-indexer-service.h
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

#ifndef IDE_INDEXER_SERVICE_H
#define IDE_INDEXER_SERVICE_H

#include <ide.h>

#include "ide-indexer-index-builder.h"
#include "ide-indexer-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDEXER_SERVICE  (ide_indexer_service_get_type())

G_DECLARE_FINAL_TYPE (IdeIndexerService, ide_indexer_service, IDE, INDEXER_SERVICE, IdeObject)

IdeIndexerIndex *ide_indexer_service_get_index (IdeIndexerService *self);

G_END_DECLS

#endif /* IDE_INDEXER_SERVICE_H */