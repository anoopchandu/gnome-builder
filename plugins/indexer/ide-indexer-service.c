#define G_LOG_DOMAIN "ide-indexer-service"

#include "ide-indexer-service.h"
#include "ide-indexer-builder.h"

/* This is a start and stop service which monitors file changes and such and
 * reindexes directories using IdeIndexerBuilder.
 */

struct _IdeIndexerService
{
  IdeObject               parent;

  /* The builder to build & update index */
  IdeIndexerBuilder      *builder;
  /* The Index which will store all declarations */
  IdeIndexerIndex        *index;

  /* Queue of directories which needs to be indexed */
  GQueue                 *building_queue;
};

typedef struct
{
  GFile   *directory;
  gboolean recursive;
}BuildingData;

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeIndexerService, ide_indexer_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
building_data_free (BuildingData *data)
{
  g_object_unref (data->directory);
  g_slice_free (BuildingData, data);
}

static void
builder_build_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeIndexerService *self = (IdeIndexerService *)user_data;
  IdeIndexerBuilder *builder = (IdeIndexerBuilder *)object;
  GError *error = NULL;

  g_assert (IDE_IS_INDEXER_SERVICE (self));

  ide_indexer_builder_build_finish (builder, result, &error);

  building_data_free (g_queue_pop_head (self->building_queue));
  g_debug ("finished building index\n");

  /* Index next directory */
  if (!g_queue_is_empty (self->building_queue))
    {
      BuildingData *bdata;

      bdata = g_queue_peek_head (self->building_queue);

      g_debug ("started building index2\n");
      ide_indexer_builder_build_async (self->builder,
                                       bdata->directory,
                                       bdata->recursive,
                                       NULL,
                                       builder_build_cb,
                                       self);
    }
}

static void
on_file_changed (IdeIndexerService *self,
                 GFile             *file)
{
  g_autoptr(GFile) parent;
  BuildingData *bdata;

  g_assert (IDE_IS_INDEXER_SERVICE (self));
  g_assert (G_IS_FILE (file));

  parent = g_file_get_parent (file);

  bdata = g_slice_new (BuildingData);
  bdata->directory = g_object_ref (parent);
  bdata->recursive = FALSE;

  if (g_queue_is_empty (self->building_queue))
    {
      g_debug ("started building index1\n");

      g_queue_push_tail (self->building_queue, bdata);
      ide_indexer_builder_build_async (self->builder,
                                       parent,
                                       FALSE,
                                       NULL,
                                       builder_build_cb,
                                       self);
    }
  else
    {
      g_queue_push_tail (self->building_queue, bdata); 
    }
}

static void
on_buffer_saved (IdeIndexerService *self,
                 IdeBuffer         *buffer,
                 IdeBufferManager  *buffer_manager)
{
  GFile *file;
  gboolean flag = FALSE;
  g_autofree gchar *file_name;

  file = ide_file_get_file (ide_buffer_get_file (buffer));
  file_name = g_file_get_uri (file);

  for (int i=0; extensions[i][0]; i++)
    {
      if (g_str_has_suffix (file_name, extensions[i]))
        {
          flag = TRUE;
          break;     
        }
    }

  if (flag)
    on_file_changed (self, file);
}

static void
on_file_trashed (IdeIndexerService *self,
                 GFile             *file,
                 IdeProject        *project)
{
  GFileType type;

  type = g_file_query_file_type (file,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 NULL);

  if (type == G_FILE_TYPE_REGULAR)
    on_file_changed (self, file);
}

static void
on_file_renamed (IdeIndexerService *self,
                 GFile             *src_file,
                 GFile             *dst_file,
                 IdeProject        *project)
{
  on_file_changed (self, dst_file);
}

static void
ide_indexer_service_context_loaded (IdeService *service)
{
  IdeIndexerService *self = (IdeIndexerService *)service;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  GFile *workdir;
  IdeProject *project;
  BuildingData *bdata;

  g_assert (IDE_IS_INDEXER_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  bufmgr = ide_context_get_buffer_manager (context);
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));

  self->index = ide_indexer_index_new (context);
  self->builder = ide_indexer_builder_new (context, self->index);
  self->building_queue = g_queue_new ();

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

  g_debug ("started building index\n");

  bdata = g_slice_new (BuildingData);
  bdata->directory = g_file_dup (workdir);
  bdata->recursive = TRUE;
  g_queue_push_tail (self->building_queue, bdata);

  ide_indexer_builder_build_async (self->builder,
                                   workdir,
                                   TRUE,
                                   NULL,
                                   builder_build_cb,
                                   self);

}

static void
ide_indexer_service_start (IdeService *service)
{
}

static void
ide_indexer_service_stop (IdeService *service)
{
  IdeIndexerService *self = (IdeIndexerService *)service;

 /* TODO: Handle stopping service while indexing. */

  g_clear_object (&self->index);
  g_clear_object (&self->builder);
  g_queue_free_full (self->building_queue, (GDestroyNotify)building_data_free);
  self->building_queue = NULL;
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
