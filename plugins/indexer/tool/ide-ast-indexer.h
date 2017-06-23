#include <glib-object.h>

#ifndef IDE_AST_INDEXER_H
#define IDE_AST_INDEXER_H

G_BEGIN_DECLS

#define IDE_TYPE_AST_INDEXER (ide_ast_indexer_get_type ())

G_DECLARE_FINAL_TYPE (IdeAstIndexer, ide_ast_indexer, IDE, AST_INDEXER, GObject)

enum SymbolType
{
  ST_STRUCT = 0,
  ST_UNION,
  ST_CLASS,
  ST_ENUM,
  ST_FIELD,
  ST_ENUMCONSTANT,
  ST_FUNCTION,
  ST_VAR,
  ST_PARAM,
  ST_TYPEDEF,
  ST_METHOD,
  ST_NAMESPACE,
  ST_FUNCTIONTEMPLATE,
  ST_CLASSTEMPLATE,
  ST_NAMESPACEALIAS,
  ST_TYPEDEFALIAS,
  ST_INVALID
};

typedef struct
{
  /* Array of number of flags for each file */
  gint       num_files;
  gint      *num_flags;
  /* Array of flags of each file */
  GPtrArray *flags;
  gchar *destination;
} IndexingData;

IdeAstIndexer*    ide_ast_indexer_new       (void);
void              ide_ast_indexer_index     (IdeAstIndexer *self,
                                             IndexingData  *idata);
void              indexing_data_free        (IndexingData  *data);
G_END_DECLS

#endif /* IDE_AST_INDEXER_H */