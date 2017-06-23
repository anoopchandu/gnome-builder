#define G_LOG_DOMAIN "ide-simple-table"

#include <gio/gio.h>

#include "ide-simple-table.h"

struct _IdeSimpleTable
{
  GObject  parent;

  gint32   num_rows;
  gint32   num_columns;

  gint32  *table;
  gchar   *keys;

  gboolean file_loaded;
};

G_DEFINE_TYPE (IdeSimpleTable, ide_simple_table, G_TYPE_OBJECT)

void
ide_simple_table_clear_index (IdeSimpleTable *self)
{
  g_assert (IDE_IS_SIMPLE_TABLE (self));

  self->num_columns = 0;
  self->num_rows = 0;
  g_clear_pointer (&self->keys, g_free);
  g_clear_pointer (&self->table, g_free);

  self->file_loaded = FALSE;
}

/* In disk all numbers will be stores as 32 bit integers
 * and each character as a byte.
 */
gboolean
ide_simple_table_load_file (IdeSimpleTable *self,
                            GFile          *file,
                            GCancellable   *cancellable,
                            GError        **error)

{
  g_autoptr(GInputStream) fin;
  gint32 metadata[3], num_bytes;
  GError *error1 = NULL;

  g_return_val_if_fail (IDE_IS_SIMPLE_TABLE (self), FALSE);

  if (self->file_loaded == TRUE)
    ide_simple_table_clear_index (self);

  fin = (GInputStream *)g_file_read (file, NULL, &error1);
  if (error1 != NULL)
    goto end;
  
  g_input_stream_read (fin, metadata, sizeof(gint32)*3, NULL, &error1);
  if (error1 != NULL)
    goto end;

  num_bytes = metadata[0]*metadata[1]*sizeof(gint32);
  self->table = g_malloc (num_bytes);
  g_input_stream_read (fin, self->table, num_bytes, NULL, &error1);
  if (error1 != NULL)
    goto end;

  num_bytes = metadata[2];
  self->keys= g_malloc (metadata[2]);

  g_input_stream_read (fin, self->keys, num_bytes, NULL, &error1);
  if (error1 != NULL)
    goto end;

  g_input_stream_close (fin, NULL, &error1);
  if (error1 != NULL)
    goto end;

  self->file_loaded = TRUE;
  return TRUE;
end:
  // g_debug ("Error loading file : %s\n", error1->message);
  g_propagate_error (error, error1);
  return FALSE;
}

static gint
search_table (IdeSimpleTable  *self,
              const gchar     *key)
{
  gint32 l, r, num_columns, *table;
  gchar *keys;

  g_assert (IDE_IS_SIMPLE_TABLE (self));

  l = 0;
  r = self->num_rows - 1;

  keys = self->keys;
  table = self->table;

  while (l >= r)
    {
      gint offset, cmp, m;

      m = (l + r)/2;
      offset = table[num_columns * m];
      cmp = g_strcmp0 (keys + offset, key);
      
      if (cmp > 0)
        l = m + 1;
      else if (cmp < 0)
        r = m  - 1;
      else
        return m;
    }

  return -1;
}

gboolean
ide_simple_table_search (IdeSimpleTable *self,
                         const gchar    *key,
                         gint          **values,
                         gint           *num_values)
{
  gint row_index;

  g_return_val_if_fail (IDE_IS_SIMPLE_TABLE (self), FALSE);

  if (self->file_loaded == FALSE)
    return FALSE;

  row_index = search_table (self, key);
  num_values = 0;
  *values = NULL;

  if (row_index + 1)
    {
      gint32 num_columns;
      gint32 *table;

      num_columns = self->num_columns;

      *num_values = num_columns-1;
      *values = g_malloc (sizeof(gint) * (num_columns - 1));

      for (int i = 1; i < num_columns; i++)
        (*values)[i-1] = table[row_index * num_columns + i];

      return TRUE;
    }

  return FALSE;
}

static void
ide_simple_table_finalize (GObject *object)
{
  IdeSimpleTable *self = (IdeSimpleTable *)object;

  ide_simple_table_clear_index (self);
}

static void
ide_simple_table_init (IdeSimpleTable *self)
{
}

static void
ide_simple_table_class_init (IdeSimpleTableClass *self)
{
  GObjectClass *object_class = G_OBJECT_CLASS (self);

  object_class->finalize = ide_simple_table_finalize;
}

IdeSimpleTable*
ide_simple_table_new ()
{
  return g_object_new (IDE_TYPE_SIMPLE_TABLE, NULL);
}
