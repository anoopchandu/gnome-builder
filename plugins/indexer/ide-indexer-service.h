#ifndef IDE_INDEXER_SERVICE_H
#define IDE_INDEXER_SERVICE_H

#include <ide.h>

#include "ide-indexer-builder.h"
#include "ide-indexer-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDEXER_SERVICE  (ide_indexer_service_get_type())

G_DECLARE_FINAL_TYPE (IdeIndexerService, ide_indexer_service, IDE, INDEXER_SERVICE, IdeObject)

IdeIndexerIndex *ide_indexer_service_get_index (IdeIndexerService *self);

G_END_DECLS

#endif /* IDE_INDEXER_SERVICE_H */