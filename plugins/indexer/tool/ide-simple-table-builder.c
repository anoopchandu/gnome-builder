#include <string.h>

#include "ide-simple-table-builder.h"

/* This is an implementation of table. Each row contains a string key and some integer values.
 * Number of values should be specified while creating table.
 *
 * All rows are stored sequentially in an GArray. Key in each row is replaced by offset in GByteArray
 * where all strings are stored sequentially. While writing into disk all rows in GArray are 
 * sorted and keys in GByteArray are kept as it is. 
 * Index format in disk:
 * -------------------------
 * NumberofRows NumberofColumns SizeofKeysArray
 * Table Array
 * Keys Array
 * -------------------------
 */

struct _IdeSimpleTableBuilder
{
  GObject      parent;

  /* array of integers */
  GArray      *table;
  /* array of characters */
  GByteArray  *keys;

  gint32       num_columns;
  gboolean     written;
};

G_DEFINE_TYPE (IdeSimpleTableBuilder, ide_simple_table_builder, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NUM_COLUMNS,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

typedef struct
{
  gchar *keys;
  gint32  *table;
  gint32   num_columns;
}CompareData;

void
ide_simple_table_builder_insert (IdeSimpleTableBuilder *self,
                                 const gchar           *key,
                                 gint32                *values)
{
  gint32 offset;
  guchar new_line = '\n';

  g_return_if_fail (IDE_IS_SIMPLE_TABLE_BUILDER (self));
  g_return_if_fail (!self->written);

  offset = self->keys->len+1;
  g_array_append_val (self->table, offset);
  g_array_append_vals (self->table, values, self->num_columns-1);

  g_byte_array_append (self->keys, (guchar *)key, strlen(key));
  g_byte_array_append (self->keys, &new_line, 1);
}

gint
compare (gint32       *row1,
         gint32       *row2,
         CompareData  *cdata)
{
  return g_strcmp0 (cdata->keys + cdata->table[(*row1) * cdata->num_columns],
                    cdata->keys + cdata->table[(*row2) * cdata->num_columns]);
}

/* This will write table into disk by sorting strings */

gboolean
ide_simple_table_builder_write (IdeSimpleTableBuilder *self,
                                GFile                 *destination,
                                GCancellable          *cancellable,
                                GError               **error)
{
  gint32 num_rows;
  GArray *proxy_table, *new_table, *old_table;
  gint32 num_columns;
  g_autoptr(GOutputStream) fout;
  gint32 *new_table_data, *old_table_data, *proxy_table_data;
  gsize bytes_written;
  CompareData cdata;
  gint32 metadata[3];

  g_return_val_if_fail (IDE_IS_SIMPLE_TABLE_BUILDER (self), FALSE);
  g_return_val_if_fail (!self->written, FALSE);

  num_rows = self->table->len/self->num_columns;
  num_columns = self->num_columns;

  /* For searching table needs to be sorted. Without sorting table array 
   * directly, a proxy table array will be sorted which will contain proxies
   * for rows in actual table. proxy for a row is it row number.
   */
  proxy_table = g_array_sized_new (FALSE, FALSE, sizeof(gint32), num_rows);

  proxy_table_data = (gint32 *)proxy_table->data;
  for (gint32 i = 0; i < num_rows; i++)
    proxy_table_data[i] = i;

  cdata.keys = (gchar *)self->keys->data;
  cdata.table = (gint32 *)self->table->data;
  cdata.num_columns = self->num_columns;
  g_array_sort_with_data (proxy_table, (GCompareDataFunc)compare, &cdata);

  old_table = self->table;
  old_table_data = (gint32 *)old_table->data;

  new_table = g_array_sized_new (FALSE, FALSE, sizeof(gint32), old_table->len);
  new_table_data = (gint32 *)new_table->data;

  for (gint32 i = 0; i < num_rows; i++)
    {
      gint32 old = proxy_table_data[i]*num_columns;
      gint32 new = i*num_columns;

      for (gint32 j = 0; j < num_columns; j++)
          new_table_data[new + j] = old_table_data[old + j];
    }

  fout = G_OUTPUT_STREAM (g_file_replace (destination, NULL, FALSE,
                          G_FILE_CREATE_NONE,
                          NULL, error));

  metadata[0] = num_rows;
  metadata[1] = num_columns;
  metadata[2] = self->keys->len;
  g_output_stream_write_all (fout, metadata, sizeof(gint32)*3,
                             &bytes_written, NULL, error);
  g_output_stream_write_all (fout, new_table_data, num_rows*num_columns*sizeof(gint32),
                             &bytes_written, NULL, error);
  g_output_stream_write_all (fout, self->keys->data, self->keys->len,
                             &bytes_written, NULL, error);

  self->written = TRUE;

  g_array_free (proxy_table, TRUE);
  g_array_free (new_table, TRUE);
  g_output_stream_close (fout, NULL, error);

  return TRUE;
}

static void
ide_simple_table_builder_constructed (GObject *object)
{
  IdeSimpleTableBuilder *self = (IdeSimpleTableBuilder *)object;

  self->table = g_array_new (FALSE, FALSE, sizeof(gint32));
  self->keys = g_byte_array_new ();
}

static void
ide_simple_table_builder_finalize (GObject *object)
{
  IdeSimpleTableBuilder *self = (IdeSimpleTableBuilder *)object;

  g_array_free (self->table, TRUE);
  g_byte_array_free (self->keys, TRUE);
}

static void
ide_simple_table_builder_init (IdeSimpleTableBuilder *self)
{
}

static void
ide_simple_table_builder_set_property (GObject       *object,
                                       guint           prop_id,
                                       const GValue  *value,
                                       GParamSpec    *pspec)
{
  IdeSimpleTableBuilder *self = (IdeSimpleTableBuilder *)object;

  switch (prop_id)
    {
    case PROP_NUM_COLUMNS:
      self->num_columns = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_simple_table_builder_get_property (GObject    *object,
                                       guint        prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeSimpleTableBuilder *self = (IdeSimpleTableBuilder *)object;

  switch (prop_id)
    {
    case PROP_NUM_COLUMNS:
      g_value_set_int (value, self->num_columns);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_simple_table_builder_class_init (IdeSimpleTableBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_simple_table_builder_constructed;
  object_class->finalize = ide_simple_table_builder_finalize;
  object_class->set_property = ide_simple_table_builder_set_property;
  object_class->get_property = ide_simple_table_builder_get_property;

  properties[PROP_NUM_COLUMNS] =
    g_param_spec_int ("num_columns",
                      "Number of Columns",
                      "Number of columns in each row.",
                      1, 100, 2,
                      G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

IdeSimpleTableBuilder*
ide_simple_table_builder_new (gint32 num_columns)
{
  IdeSimpleTableBuilder *self;

  self = g_object_new (IDE_TYPE_SIMPLE_TABLE_BUILDER,
                       "num_columns", num_columns,
                       NULL);

  return self;
}
