#include <dazzle.h>
#include <clang-c/Index.h>
#include <stdlib.h>

#include "ide-ast-indexer.h"
#include "ide-simple-table-builder.h"

/* This class will traverse AST of given set of files, extract keys for global
 * definitions and names of all symbols and put that into IdeSimpleTable and 
 * DzlFuzzyIndex respectively
 *
 */

struct _IdeAstIndexer
{
  GObject parent; 

  CXIndex index;
};

typedef struct
{
  IdeAstIndexer         *source_object;
  DzlFuzzyIndexBuilder  *index_builder;
  IdeSimpleTableBuilder *table_builder;
  gint                   file_id;
  gchar                 *file_path;
  gint                   num_flags;
  gchar                **flags;
} TaskData;

G_DEFINE_TYPE (IdeAstIndexer, ide_ast_indexer, G_TYPE_OBJECT)

static void
task_data_free (TaskData *data)
{
  g_object_unref (data->index_builder);
  g_object_unref (data->table_builder);
  g_slice_free (TaskData, data);
}
void
indexing_data_free (IndexingData *data)
{
  g_ptr_array_free (data->flags, TRUE);
  g_free (data->num_flags);
  g_free (data->destination);
  g_slice_free (IndexingData, data);
}

static enum CXChildVisitResult
visitor (CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  enum CXCursorKind cursor_kind;
  TaskData *task_data = client_data;

  g_assert (!clang_Cursor_isNull (cursor));
  g_assert (task_data != NULL);

  cursor_kind = clang_getCursorKind (cursor);

  if ((cursor_kind >= CXCursor_StructDecl && cursor_kind <= CXCursor_Namespace) ||
      (cursor_kind >= CXCursor_Constructor && cursor_kind <= CXCursor_NamespaceAlias) ||
      cursor_kind == CXCursor_TypeAliasDecl ||
      cursor_kind == CXCursor_MacroDefinition)
    {
      CXSourceLocation location;
      guint line, column, offset;
      CXFile file;
      CXString cx_file_path;
      const char *file_path;

      location = clang_getCursorLocation (cursor);
      clang_getSpellingLocation (location, &file, &line, &column, &offset);

      cx_file_path = clang_getFileName (file);
      file_path = clang_getCString (cx_file_path);

      if (file_path != NULL && g_str_equal (file_path, task_data->file_path))
        {
          gchar name[100];
          gchar flag = 0, type = 0;
          enum CXLinkageKind linkage;
          CXString spelling;
          gchar definition;

          definition = !!clang_isCursorDefinition (cursor);
          flag = flag | definition;

          linkage = clang_getCursorLinkage (cursor);
          if (linkage != CXLinkage_NoLinkage && linkage != CXLinkage_Internal)
            {
              CXString USR;
              const gchar *key;
              int idata[5];

              USR = clang_getCursorUSR (cursor);
              key = clang_getCString (USR);

              idata[0] = task_data->file_id;
              idata[1] = line;
              idata[2] = column;
              idata[3] = flag;
              idata[4] = definition;

              ide_simple_table_builder_insert (task_data->table_builder, key, idata);

              flag = flag | 2;
              clang_disposeString (USR);
            }

          switch (cursor_kind)
            {
            case CXCursor_StructDecl:
              type = ST_STRUCT;
              break;
            case CXCursor_UnionDecl:
              type = ST_UNION;
              break;
            case CXCursor_ClassDecl:
              type = ST_CLASS;
              break;
            case CXCursor_EnumDecl:
              type = ST_ENUM;
              break;
            case CXCursor_FieldDecl:
              type = ST_FIELD;
              break;
            case CXCursor_EnumConstantDecl:
              type = ST_ENUMCONSTANT;
              break;
            case CXCursor_FunctionDecl:
              type = ST_FUNCTION;
              break;
            case CXCursor_VarDecl:
              type = ST_VAR;
            case CXCursor_ParmDecl:
              type = ST_PARAM;
              break;
            case CXCursor_TypedefDecl:
              type = ST_TYPEDEF;
              break;
            case CXCursor_CXXMethod:
              type = ST_METHOD;
              break;
            case CXCursor_Namespace:
              type = ST_NAMESPACE;
              break;
            case CXCursor_FunctionTemplate:
              type = ST_FUNCTIONTEMPLATE;
              break;
            case CXCursor_ClassTemplate:
              type = ST_CLASSTEMPLATE;
              break;
            case CXCursor_NamespaceAlias:
              type = ST_NAMESPACEALIAS;
              break;
            case CXCursor_TypeAliasDecl:
              type = ST_TYPEDEFALIAS;
              break;
            default:
              break;
            }

          /* first two characters in name will store symbol type and
           * its attributes like local/not declaration/definition
           */
          name[0] = type;
          name[1] = flag;
          spelling = clang_getCursorSpelling (cursor);

          g_strlcpy (name + 2, clang_getCString (spelling), 90);

          dzl_fuzzy_index_builder_insert (task_data->index_builder,
                                          name,
                                          g_variant_new ("(uuuu)",
                                          task_data->file_id,
                                          line, column, offset),
                                          0);

          clang_disposeString (spelling);
        }
        clang_disposeString (cx_file_path);
    }
  else if (cursor_kind == CXCursor_MacroExpansion)
    {
      /* TODO: Record MACRO EXPANSION FOR G_DEFINE_TYPE, G_DECLARE_TYPE */
    }

  return CXChildVisit_Recurse;
}

/* This function will extract declaraions from a file and put that into
 * destination folder
 */
static void
ide_ast_indexer_index_file (IdeAstIndexer *self,
                            TaskData      *task_data)
{
  CXTranslationUnit tu;
  CXCursor root_cursor;

  clang_parseTranslationUnit2 (self->index,
                               task_data->file_path,
                               (const char * const *) task_data->flags,
                               task_data->num_flags,
                               NULL, 0,
                               CXTranslationUnit_DetailedPreprocessingRecord,
                               &tu);

  if (tu == NULL)
    return;

  root_cursor = clang_getTranslationUnitCursor (tu);

  clang_visitChildren (root_cursor, visitor, task_data);

  clang_disposeTranslationUnit (tu);
}

/* This function indexes all source files in input and 
 * put that into destination folder using helper functions.
 */
void
ide_ast_indexer_index (IdeAstIndexer   *self,
                       IndexingData    *idata)
{
  TaskData *task_data;
  gint *num_flags, num_files;
  gchar **flags;
  g_autoptr(GFile) dest_dir = NULL, dest_file1 = NULL, dest_file2 = NULL;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_AST_INDEXER (self));

  task_data = g_slice_new (TaskData);

  task_data->source_object = self;
  task_data->index_builder = dzl_fuzzy_index_builder_new ();
  task_data->table_builder = ide_simple_table_builder_new (6/*Num columns*/);

  num_files = idata->num_files;
  num_flags = idata->num_flags;
  flags = (gchar **)idata->flags->pdata;

  dzl_fuzzy_index_builder_set_metadata_uint32 (task_data->index_builder, "NumFiles", num_files);

  for (gint i = 0; i < num_files; i++)
    {
      g_autofree gchar *base_name;

      task_data->file_id = i + 1;
      task_data->file_path = flags[0];
      task_data->flags = flags + 1;
      task_data->num_flags = num_flags[i] - 1;

      ide_ast_indexer_index_file (self, task_data);
      
      base_name = g_path_get_basename (flags[0]);
      g_print ("%s\n", flags[0]);
      
      dzl_fuzzy_index_builder_set_metadata_uint32 (task_data->index_builder, base_name, i + 1);

      flags += num_flags[i];
    }

  dest_dir = g_file_new_for_path (idata->destination);

  dest_file1 = g_file_get_child (dest_dir, "index1");
  ide_simple_table_builder_write (task_data->table_builder, dest_file1, NULL, &error);
  
  g_clear_error (&error);

  dest_file2 = g_file_get_child (dest_dir, "index2");
  dzl_fuzzy_index_builder_write (task_data->index_builder, dest_file2,
                                 G_PRIORITY_HIGH, NULL, &error);
  
  g_clear_error (&error);
  task_data_free (task_data);
  indexing_data_free (idata);
}

static void
ide_ast_indexer_finalize (GObject *object)
{
  IdeAstIndexer *self = (IdeAstIndexer *)object;

  clang_disposeIndex (self->index);
}

static void
ide_ast_indexer_init (IdeAstIndexer *self)
{
  self->index = clang_createIndex (0, 0);
}

static void
ide_ast_indexer_class_init (IdeAstIndexerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ast_indexer_finalize;
}

IdeAstIndexer*
ide_ast_indexer_new (void)
{
  return g_object_new (IDE_TYPE_AST_INDEXER, NULL);
}
