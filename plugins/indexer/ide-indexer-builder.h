#ifndef IDE_INDEXER_BUILDER_H
#define IDE_INDEXER_BUILDER_H

#include <ide.h>

#include "ide-indexer-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDEXER_BUILDER (ide_indexer_builder_get_type ())

G_DECLARE_FINAL_TYPE (IdeIndexerBuilder, ide_indexer_builder, IDE, INDEXER_BUILDER, IdeObject)

IdeIndexerBuilder *ide_indexer_builder_new          (IdeContext              *context,
                                                                IdeIndexerIndex         *index);
void                    ide_indexer_builder_build_async  (IdeIndexerBuilder  *self,
                                                                GFile                   *directory,
                                                                gboolean                 recursive,
                                                                GCancellable            *cancellable,
                                                                GAsyncReadyCallback      callback,
                                                                gpointer                 user_data);
gboolean                ide_indexer_builder_build_finish (IdeIndexerBuilder  *self,
                                                                GAsyncResult            *result,
                                                                GError                 **error);

extern const gchar *extensions[];

G_END_DECLS

#endif /* IDE_INDEXER_BUILDER_H */