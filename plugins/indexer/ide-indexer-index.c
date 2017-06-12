/* ide-indexer-index.c
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

#define G_LOG_DOMAIN "ide-indexer-index"

#include "ide-indexer-index.h"

struct _IdeIndexerIndex
{
  IdeObject   parent;

  GHashTable *indexes;
  GHashTable *fileids;
};

enum
{
  PROP_0,
  PROP_FILEIDS,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE (IdeIndexerIndex, ide_indexer_index, IDE_TYPE_OBJECT)

static gboolean
indexer_file_equal (gconstpointer a, 
                    gconstpointer b)
{
  return g_file_equal ((GFile *)a, (GFile *)b);
}

gboolean
ide_indexer_index_insert (IdeIndexerIndex *self,
                          GFile           *file)
{
  g_return_val_if_fail (IDE_IS_INDEXER_INDEX (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_hash_table_lookup (self->indexes, file) == NULL)
    {
      g_hash_table_insert (self->indexes, g_object_ref (file), dzl_fuzzy_index_new ());
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

void
ide_indexer_index_load_file_async (IdeIndexerIndex     *self,
                                   GFile               *file,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  dzl_fuzzy_index_load_file_async (g_hash_table_lookup (self->indexes, file),
                                   file, cancellable, callback, user_data);
}

gboolean 
ide_indexer_index_load_file_finish (DzlFuzzyIndex   *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  return dzl_fuzzy_index_load_file_finish (self, result, error);
}

gboolean
ide_indexer_index_fetch_declaration (IdeIndexerIndex *self, 
                                     const gchar     *key,
                                     guint           *fileid,
                                     guint           *line,
                                     guint           *coulmn)
{
  return FALSE;
}

void 
ide_indexer_index_fuzzy_search (IdeIndexerIndex *self,
                                const gchar     *key,
                                GList           *list)
{

}

static void
ide_indexer_index_set_property (GObject      *object,
                                guint         propid,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeIndexerIndex *self = (IdeIndexerIndex *)object;

  switch (propid)
    {
    case PROP_FILEIDS:
      self->fileids = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
    }
}

static void
ide_indexer_index_get_property (GObject    *object,
                                guint       propid,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeIndexerIndex *self = (IdeIndexerIndex *)object;

  switch (propid)
    {
    case PROP_FILEIDS:
      g_value_set_pointer (value, self->fileids);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
    }
}

static void
ide_indexer_index_dispose (GObject *object)
{
  IdeIndexerIndex *self = (IdeIndexerIndex *)object;

  g_hash_table_unref (self->indexes);
}

static void
ide_indexer_index_init (IdeIndexerIndex *self)
{
  self->indexes = g_hash_table_new_full (g_file_hash, indexer_file_equal, 
                                         g_object_unref, g_object_unref);
}

static void
ide_indexer_index_class_init (IdeIndexerIndexClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->set_property = ide_indexer_index_set_property;
  object_class->get_property = ide_indexer_index_get_property;
  object_class->dispose = ide_indexer_index_dispose;

  properties[PROP_FILEIDS] =
    g_param_spec_pointer ("fileids",
                          "File Ids",
                          "Table of File and its Id in index",
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

IdeIndexerIndex *
ide_indexer_index_new (GHashTable *fileids)
{
  return g_object_new (IDE_TYPE_INDEXER_INDEX,
                       "fileids", fileids,
                       NULL);
}