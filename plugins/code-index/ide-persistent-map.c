/* ide-persistent-map.c
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

#define G_LOG_DOMAIN "ide-persistent-map"

#include <ide.h>

#include "ide-persistent-map.h"

typedef struct
{
  guint32 key;
  guint32 value;
} KVPair;

struct _IdePersistentMap
{
  GObject            parent;

  GMappedFile       *mapped_file;

  GVariant          *data;

  GVariant          *keys_var;
  const gchar       *keys;

  GVariant          *values;

  GVariant          *kvpairs_var;
  const KVPair      *kvpairs;

  GVariantDict      *metadata;

  gsize              n_kvpairs;

  gboolean           loaded : 1;

};

G_DEFINE_TYPE (IdePersistentMap, ide_persistent_map, G_TYPE_OBJECT)

static void
ide_persistent_map_load_file_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdePersistentMap *self = source_object;
  GFile *file = task_data;
  g_autofree gchar *path = NULL;
  g_autoptr(GMappedFile) mapped_file = NULL;
  g_autoptr(GVariant) data = NULL;
  g_autoptr(GVariant) keys = NULL;
  g_autoptr(GVariant) values = NULL;
  g_autoptr(GVariant) metadata = NULL;
  g_autoptr(GVariant) kvpairs = NULL;
  g_autoptr(GVariant) value_size = NULL;
  GVariantDict dict;
  gint version;
  gsize n_elements;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_PERSISTENT_MAP (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->loaded)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Index already loaded");
      return;
    }

  self->loaded = TRUE;

  if (!g_file_is_native (file) || NULL == (path = g_file_get_path (file)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "Index must be a local file");
      return;
    }

  if (NULL == (mapped_file = g_mapped_file_new (path, FALSE, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT,
                                  g_mapped_file_get_contents (mapped_file),
                                  g_mapped_file_get_length (mapped_file),
                                  FALSE, NULL, NULL);

  if (data == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Failed to parse GVariant");
      return;
    }

  g_variant_ref_sink (data);

  g_variant_dict_init (&dict, data);

  if (!g_variant_dict_lookup (&dict, "version", "i", &version) || version != 1)
    {
      g_variant_dict_clear (&dict);
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Version mismatch in gvariant. Got %d, expected 1",
                               version);
      return;
    }

  keys = g_variant_dict_lookup_value (&dict, "keys", G_VARIANT_TYPE_ARRAY);
  values = g_variant_dict_lookup_value (&dict, "values", G_VARIANT_TYPE_ARRAY);
  kvpairs = g_variant_dict_lookup_value (&dict, "kvpairs", G_VARIANT_TYPE_ARRAY);
  metadata = g_variant_dict_lookup_value (&dict, "metadata", G_VARIANT_TYPE_VARDICT);

  g_variant_dict_clear (&dict);

  if (keys == NULL || values == NULL || kvpairs == NULL || metadata == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Invalid GVariant index");
      return;
    }

  self->keys = g_variant_get_fixed_array (keys,
                                          &n_elements,
                                          sizeof (guint8));

  self->kvpairs = g_variant_get_fixed_array (kvpairs,
                                             &self->n_kvpairs,
                                             sizeof (KVPair));

  self->mapped_file = g_steal_pointer (&mapped_file);
  self->data = g_steal_pointer (&data);
  self->keys_var = g_steal_pointer (&keys);
  self->values = g_steal_pointer (&values);
  self->kvpairs_var = g_steal_pointer (&kvpairs);
  self->metadata = g_variant_dict_new (metadata);

  g_task_return_boolean (task, TRUE);
}

gboolean
ide_persistent_map_load_file (IdePersistentMap *self,
                              GFile            *file,
                              GCancellable     *cancellable,
                              GError          **error)
{
  g_autoptr(GTask) task = NULL;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  task = g_task_new (self, cancellable, NULL, NULL);

  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_persistent_map_load_file);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);

  ide_persistent_map_load_file_worker (task, self, file, cancellable);

  return g_task_propagate_boolean (task, error);
}

void
ide_persistent_map_load_file_async  (IdePersistentMap                *self,
                                     GFile                 *file,
                                     GCancellable          *cancellable,
                                     GAsyncReadyCallback    callback,
                                     gpointer               user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_PERSISTENT_MAP (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_set_source_tag (task, ide_persistent_map_load_file_async);

  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task, ide_persistent_map_load_file_worker);
}

gboolean
ide_persistent_map_load_file_finish (IdePersistentMap        *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

GVariant *
ide_persistent_map_lookup_value (IdePersistentMap       *self,
                                 const gchar  *key)
{
  glong l;
  glong r;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  l = 0;
  r = self->n_kvpairs - 1; /* unsigned long to signed long */

  while (l <= r)
    {
      glong m;
      gint cmp;

      m = (l + r)/2;

      cmp = g_strcmp0 (key, &self->keys [self->kvpairs [m].key]);

      if (cmp < 0)
        r = m - 1;
      else if (cmp > 0)
        l = m + 1;
      else
        return g_variant_get_child_value (self->values, self->kvpairs [m].value);
    }

  return NULL;
}

gint64
ide_persistent_map_builder_get_metadata_int64 (IdePersistentMap      *self,
                                               const gchar *key)
{
  g_autoptr (GVariant) value = NULL;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), 0);

  value = g_variant_dict_lookup_value (self->metadata, key, G_VARIANT_TYPE_UINT64);

  if (value != NULL)
    return g_variant_get_uint64 (value);

  return 0;
}

static void
ide_persistent_map_finalize (GObject *object)
{
  IdePersistentMap *self = (IdePersistentMap *)object;

  g_clear_pointer (&self->mapped_file, g_mapped_file_unref);
  g_clear_pointer (&self->keys_var, g_variant_unref);
  g_clear_pointer (&self->values, g_variant_unref);
  g_clear_pointer (&self->kvpairs_var, g_variant_unref);
  g_clear_pointer (&self->metadata, g_variant_dict_unref);

  G_OBJECT_CLASS (ide_persistent_map_parent_class)->finalize (object);
}

static void
ide_persistent_map_init (IdePersistentMap *self)
{
}

static void
ide_persistent_map_class_init (IdePersistentMapClass *self)
{
  GObjectClass *object_class = G_OBJECT_CLASS (self);

  object_class->finalize = ide_persistent_map_finalize;
}

IdePersistentMap*
ide_persistent_map_new ()
{
  return g_object_new (IDE_TYPE_PERSISTENT_MAP, NULL);
}
