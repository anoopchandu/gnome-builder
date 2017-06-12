/* ide-indexer-service.c
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

#define G_LOG_DOMAIN "ide-indexer-service"

#include "ide-indexer-service.h"
#include "ide-indexer-index-builder.h"

struct _IdeIndexerService
{
  IdeObject               parent;

  /* The builder to build & update index */
  IdeIndexerIndexBuilder *index_builder;
  /* The Index which will store all declarations */
  IdeIndexerIndex        *index;

  GHashTable             *fileids;
  /* Queue of directories which will be indexed (recursively) one by one */
  GQueue                 *dirs_queue;
  GQueue                 *recursive_queue;
};

static void service_iface_init (IdeServiceInterface *iface);
static void ide_indexer_service_build_for_directory (IdeIndexerService *self);

G_DEFINE_TYPE_EXTENDED (IdeIndexerService, ide_indexer_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static gboolean
indexer_file_equal (gconstpointer a, 
                    gconstpointer b)
{
  return g_file_equal ((GFile *)a, (GFile *)b);
}

static void
index_builder_build_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeIndexerService *self = (IdeIndexerService *)user_data;
  IdeIndexerIndexBuilder *index_builder = (IdeIndexerIndexBuilder *)object;
  GError *error = NULL;
  g_autofree char *file_uri;

  g_assert (IDE_IS_INDEXER_SERVICE (self));

  if (ide_indexer_index_builder_build_finish (index_builder, result, &error))
    g_debug ("Indexing success\n");
  else
    g_debug ("Indexing failed\n");

  /* Index next directory */
  if (!g_queue_is_empty (self->dirs_queue))
    ide_indexer_service_build_for_directory (self);
}

static void
ide_indexer_service_build_for_directory (IdeIndexerService *self)
{
  g_autoptr(GFile) directory;
  gboolean recursive;

  g_assert (IDE_IS_INDEXER_SERVICE (self));
  
  if (g_queue_is_empty (self->dirs_queue))
    return;

  recursive = GPOINTER_TO_UINT (g_queue_pop_head (self->recursive_queue));
  directory = g_queue_pop_head (self->dirs_queue);

  ide_indexer_index_builder_build_async (self->index_builder,
                                         directory,
                                         recursive,
                                         NULL,
                                         index_builder_build_cb,
                                         self);
}

static void
on_file_changed (IdeIndexerService *self,
                 GFile             *file)
{
  g_autoptr(GFile) parent;
  GQueue *queue;
  GQueue *recursive_queue;

  g_assert (IDE_IS_INDEXER_SERVICE (self));
  g_assert (G_IS_FILE (file));

  parent = g_file_get_parent (file);
  queue = self->dirs_queue;
  recursive_queue = self->recursive_queue;

  if (g_queue_is_empty (queue))
    {
      g_queue_push_tail (recursive_queue, GUINT_TO_POINTER (0));
      g_queue_push_tail (queue, g_object_ref (parent));

      ide_indexer_service_build_for_directory (self);
    }
  else
    {
      g_queue_push_tail (recursive_queue, GUINT_TO_POINTER (0));
      g_queue_push_tail (queue, g_object_ref (parent));
    }
}

static void
on_buffer_saved (IdeIndexerService *self,
                 IdeBuffer         *buffer,
                 IdeBufferManager  *buffer_manager)
{
  on_file_changed (self, ide_file_get_file (ide_buffer_get_file (buffer)));
}

static void
on_file_trashed (IdeIndexerService *self,
                 GFile             *file,
                 IdeProject        *project)
{
  on_file_changed (self, file);
}

static void
on_file_renamed (IdeIndexerService *self,
                 GFile             *src_file,
                 GFile             *dst_file,
                 IdeProject        *project)
{
  GHashTable *fileids;
  gpointer fileid;

  g_assert (IDE_IS_INDEXER_SERVICE (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));

  fileids = self->fileids;

  fileid = g_hash_table_lookup (fileids, src_file);

  g_hash_table_remove (fileids, src_file);

  g_hash_table_insert (fileids, g_file_dup (dst_file), fileid);
}

static void
ide_indexer_service_context_loaded (IdeService *service)
{
  IdeIndexerService *self = (IdeIndexerService *)service;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  GFile *workdir;
  IdeProject *project;

  g_assert (IDE_IS_INDEXER_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  bufmgr = ide_context_get_buffer_manager (context);
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));

  self->fileids = g_hash_table_new_full (g_file_hash, indexer_file_equal,
                                         g_object_unref, NULL);

  self->index = ide_indexer_index_new (self->fileids);
  self->index_builder = ide_indexer_index_builder_new (context, self->fileids, self->index);
  self->dirs_queue = g_queue_new ();
  self->recursive_queue = g_queue_new ();

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (on_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (on_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (on_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  g_queue_push_tail (self->recursive_queue, GUINT_TO_POINTER (1));
  g_queue_push_tail (self->dirs_queue, g_file_dup (workdir));

  ide_indexer_service_build_for_directory (self);
}

static void
ide_indexer_service_start (IdeService *service)
{
}

static void
ide_indexer_service_stop (IdeService *service)
{
  IdeIndexerService *self = (IdeIndexerService *)service;

  g_hash_table_unref (self->fileids);
  g_clear_object (&self->index);
  g_clear_object (&self->index_builder);
  g_queue_free_full (self->dirs_queue, g_object_unref);
}

static void
ide_indexer_service_class_init (IdeIndexerServiceClass *klass)
{
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->start = ide_indexer_service_start;
  iface->context_loaded = ide_indexer_service_context_loaded;
  iface->stop = ide_indexer_service_stop;
}

static void
ide_indexer_service_init (IdeIndexerService *self)
{
}

IdeIndexerIndex *
ide_indexer_service_get_index (IdeIndexerService *self)
{
  g_return_val_if_fail (IDE_IS_INDEXER_SERVICE (self), NULL);

  return self->index;
}
