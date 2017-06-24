#ifndef IDE_INDEXER_INDEX_H
#define IDE_INDEXER_INDEX_H

#include <ide.h>

#include "tool/ide-simple-table.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDEXER_INDEX (ide_indexer_index_get_type())

typedef struct
{
  DzlFuzzyIndex *names;
  IdeSimpleTable *keys;
} DirectoryIndex;

G_DECLARE_FINAL_TYPE (IdeIndexerIndex, ide_indexer_index, IDE, INDEXER_INDEX, IdeObject)

DirectoryIndex  *ide_indexer_index_load_index           (IdeIndexerIndex     *self,
                                                         GFile               *directory,
                                                         GCancellable        *cancellable,
                                                         GError             **error);
void             ide_indexer_index_load_indexes_async   (IdeIndexerIndex     *self,
                                                         GPtrArray           *directories,
                                                         GCancellable        *cancellable,
                                                         GAsyncReadyCallback  callback,
                                                         gpointer             user_data);
gboolean         ide_indexer_index_load_indexes_finish  (IdeIndexerIndex     *self,
                                                         GAsyncResult        *result,
                                                         GError             **error);
gboolean         ide_indexer_index_fetch_declaration    (IdeIndexerIndex     *self, 
                                                         const gchar         *key,
                                                         guint               *fileid,
                                                         guint               *line,
                                                         guint               *coulmn);
void             ide_indexer_index_fuzzy_search         (IdeIndexerIndex     *self,
                                                         const gchar         *key,
                                                         GList               *list);
IdeIndexerIndex *ide_indexer_index_new                  (IdeContext          *context);

G_END_DECLS

#endif /* IDE_INDEXER_INDEX_H */
