/* ide-code-index-service.c
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

#define G_LOG_DOMAIN "ide-code-index-service"

#include <libpeas/peas.h>
#include <stdlib.h>
#include <plugins/ide-extension-adapter.h>

#include "ide-code-index-service.h"
#include "ide-code-index-builder.h"

/*
 * This is a start and stop service which monitors file changes and
 * reindexes directories using IdeCodeIndexBuilder.
 */

struct _IdeCodeIndexService
{
  IdeObject               parent;

  /* The builder to build & update index */
  IdeCodeIndexBuilder    *builder;
  /* The Index which will store all declarations */
  IdeCodeIndexIndex      *index;

  /* Queue of directories which needs to be indexed */
  GQueue                  build_queue;
  GHashTable             *build_dirs;

  GHashTable             *code_indexers;
  // IdeExtensionSetAdapter *adapter;

  GCancellable           *cancellable;
  gboolean                stopped : 1;
};

typedef struct
{
  IdeCodeIndexService *self;
  GFile               *directory;
  guint                recursive : 1;
} BuildData;

static void service_iface_init           (IdeServiceInterface *iface);
static void ide_code_index_service_build (IdeCodeIndexService *self,
                                          GFile               *directory,
                                          gboolean             recursive);

G_DEFINE_TYPE_EXTENDED (IdeCodeIndexService, ide_code_index_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
remove_source (gpointer source_id)
{
  g_source_remove (GPOINTER_TO_UINT (source_id));
}

static void
build_data_free (BuildData *data,
                 gpointer   user_data)
{
  g_clear_object (&data->directory);
  g_slice_free (BuildData, data);
}

// static void
// indexer_free (gpointer *indexer)
// {
//   if (indexer != NULL)
//     g_object_unref (indexer);
// }

static void
ide_code_index_service_build_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(IdeCodeIndexService) self = (IdeCodeIndexService *)user_data;
  IdeCodeIndexBuilder *builder = (IdeCodeIndexBuilder *)object;
  g_autoptr(GError) error = NULL;
  BuildData *bdata;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (builder));

  if (self->stopped)
    return;

  bdata = g_queue_pop_head (&self->build_queue);

  if (ide_code_index_builder_build_finish (builder, result, &error))
    {
      g_message ("Finished building index");
    }
  else
    {
      g_message ("Failed to build index, %s, retrying", error->message);

      ide_code_index_service_build (self, bdata->directory, bdata->recursive);
    }

  build_data_free (bdata, NULL);

  /* Index next directory */
  if (!g_queue_is_empty (&self->build_queue))
    {
      bdata = g_queue_peek_head (&self->build_queue);

      g_clear_object (&self->cancellable);

      self->cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (self->builder,
                                          bdata->directory,
                                          bdata->recursive,
                                          self->cancellable,
                                          ide_code_index_service_build_cb,
                                          g_object_ref (self));
    }
}

static gboolean
ide_code_index_serivce_push (BuildData *bdata)
{
  IdeCodeIndexService *self;

  g_assert (bdata != NULL);

  self = bdata->self;

  g_hash_table_remove (self->build_dirs, bdata->directory);

  if (g_queue_is_empty (&self->build_queue))
    {
      g_queue_push_tail (&self->build_queue, bdata);

      g_clear_object (&self->cancellable);

      self->cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (self->builder,
                                          bdata->directory,
                                          bdata->recursive,
                                          self->cancellable,
                                          ide_code_index_service_build_cb,
                                          g_object_ref (self));
    }
  else
    {
      g_queue_push_tail (&self->build_queue, bdata);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_code_index_service_build (IdeCodeIndexService *self,
                              GFile               *directory,
                              gboolean             recursive)
{
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (directory));

  if (!g_hash_table_lookup (self->build_dirs, directory))
    {
      BuildData *bdata;
      guint source_id;

      bdata = g_slice_new0 (BuildData);
      bdata->self = self;
      bdata->directory = g_object_ref (directory);
      bdata->recursive = recursive;

      source_id = g_timeout_add_seconds (5, (GSourceFunc)ide_code_index_serivce_push, bdata);

      g_hash_table_insert (self->build_dirs,
                           g_object_ref (directory),
                           GUINT_TO_POINTER (source_id));
    }
}

static void
ide_code_index_service_vcs_changed (IdeCodeIndexService *self,
                                    IdeVcs              *vcs)
{
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_VCS (vcs));

  ide_code_index_service_build (self, ide_vcs_get_working_directory (vcs), TRUE);
}

static void
ide_code_index_service_buffer_saved (IdeCodeIndexService *self,
                                     IdeBuffer           *buffer,
                                     IdeBufferManager    *buffer_manager)
{
  GFile *file;
  g_autofree gchar *file_name = NULL;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_file_get_file (ide_buffer_get_file (buffer));
  file_name = g_file_get_uri (file);

  if (NULL != ide_code_index_service_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      ide_code_index_service_build (self, parent, FALSE);
    }
}

static void
ide_code_index_service_file_trashed (IdeCodeIndexService *self,
                                     GFile               *file,
                                     IdeProject          *project)
{
  g_autofree gchar *file_name = NULL;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (file));

  file_name = g_file_get_uri (file);

  if (NULL != ide_code_index_service_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      ide_code_index_service_build (self, parent, FALSE);
    }
}

static void
ide_code_index_service_file_renamed (IdeCodeIndexService *self,
                                     GFile               *src_file,
                                     GFile               *dst_file,
                                     IdeProject          *project)
{
  g_autofree gchar *src_file_name = NULL;
  g_autofree gchar *dst_file_name = NULL;
  g_autoptr(GFile) src_parent = NULL;
  g_autoptr(GFile) dst_parent = NULL;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));

  src_file_name = g_file_get_uri (src_file);
  dst_file_name = g_file_get_uri (dst_file);

  src_parent = g_file_get_parent (src_file);
  dst_parent = g_file_get_parent (dst_file);

  if (g_file_equal (src_parent, dst_parent))
    {
      if (NULL != ide_code_index_service_get_code_indexer (self, src_file_name) ||
          NULL != ide_code_index_service_get_code_indexer (self, dst_file_name))
        ide_code_index_service_build (self, src_parent, FALSE);
    }
  else
    {
      if (NULL != ide_code_index_service_get_code_indexer (self, src_file_name))
        ide_code_index_service_build (self, src_parent, FALSE);

      if (NULL != ide_code_index_service_get_code_indexer (self, dst_file_name))
        ide_code_index_service_build (self, dst_parent, FALSE);
    }
}

// static void
// ide_code_index_service_extension_added (IdeExtensionSetAdapter *adapter,
//                                         PeasPluginInfo         *plugin_info,
//                                         PeasExtension          *extension,
//                                         gpointer                user_data)
// {
//   IdeCodeIndexService *self = (IdeCodeIndexService *)user_data;
//   const gchar *value;
//   g_auto(GStrv) languages = NULL;

//   g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
//   g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

//   value = peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages");
//   languages = g_strsplit (value, ",", 0);

//   for (int i = 0; languages [i] != NULL; i++)
//     g_hash_table_insert (self->code_indexers,
//                          g_strdup (languages[i]), /* steal pointer */
//                          g_object_ref (IDE_CODE_INDEXER (extension)));
// }

// static void
// ide_code_index_service_extension_removed (IdeExtensionSetAdapter *adapter,
//                                           PeasPluginInfo         *plugin_info,
//                                           PeasExtension          *extension,
//                                           gpointer                user_data)
// {
//   IdeCodeIndexService *self = (IdeCodeIndexService *)user_data;
//   const gchar *value;
//   g_auto(GStrv) languages = NULL;

//   g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
//   g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

//   value = peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages");
//   languages = g_strsplit (value, ",", 0);

//   for (int i = 0; languages [i] != NULL; i++)
//       g_hash_table_remove (self->code_indexers, languages[i]);
// }

static void
ide_code_index_service_context_loaded (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;
  IdeContext *context;
  IdeProject *project;
  IdeBufferManager *bufmgr;
  IdeVcs *vcs;
  GFile *workdir;
  const GList *plugins;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  bufmgr = ide_context_get_buffer_manager (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->code_indexers = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               g_object_unref);

  plugins = peas_engine_get_plugin_list (peas_engine_get_default ());

  while (plugins != NULL)
    {
      PeasPluginInfo *plugin_info;
      const gchar *value;

      plugin_info = plugins->data;

      value = peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages");

      if (value != NULL)
        {
          g_autoptr(IdeExtensionAdapter) adapter = NULL;
          g_auto(GStrv) languages = NULL;

          languages = g_strsplit (value, ",", 0);

          adapter = ide_extension_adapter_new (context,
                                               NULL,
                                               IDE_TYPE_CODE_INDEXER,
                                               "Code-Indexer-Languages",
                                               languages[0]);

          for (int i = 0; languages[i] != NULL; i++)
            {
              g_hash_table_insert (self->code_indexers,
                                   g_strdup (languages[i]),
                                   g_object_ref (adapter));
            }
        }
        plugins = plugins->next;
    }

  g_message ("extensions created");
  // self->adapter = ide_extension_set_adapter_new (ide_object_get_context (IDE_OBJECT (self)),
  //                                                peas_engine_get_default (),
  //                                                IDE_TYPE_CODE_INDEXER,
  //                                                NULL,
  //                                                NULL);

  // g_signal_connect (self->adapter,
  //                   "extension-added",
  //                   G_CALLBACK (ide_code_index_service_extension_added),
  //                   self);  if self destroyed
  // g_signal_connect (self->adapter,
  //                   "extension-removed",
  //                   G_CALLBACK (ide_code_index_service_extension_removed),
  //                   self);

  self->index = ide_code_index_index_new (context);
  self->builder = ide_code_index_builder_new (context, self->index, self);
  self->build_dirs = g_hash_table_new_full (g_file_hash,
                                            (GEqualFunc)g_file_equal,
                                            g_object_unref,
                                            remove_source);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (ide_code_index_service_vcs_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (ide_code_index_service_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (ide_code_index_service_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (ide_code_index_service_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_code_index_service_build (self, workdir, TRUE);

  g_message ("context loaded");
}

static void
ide_code_index_service_start (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;

  self->stopped = FALSE;

  g_message ("service started");
}

static void
ide_code_index_service_stop (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);

  self->stopped = TRUE;

  g_clear_object (&self->index);
  g_clear_object (&self->builder);
  g_queue_foreach (&self->build_queue, (GFunc)build_data_free, NULL);
  g_queue_clear (&self->build_queue);
  g_clear_pointer (&self->build_dirs, g_hash_table_unref);
  g_clear_pointer (&self->code_indexers, g_hash_table_unref);

  g_message ("service stopped");
}

static void
ide_code_index_service_class_init (IdeCodeIndexServiceClass *klass)
{
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->start = ide_code_index_service_start;
  iface->context_loaded = ide_code_index_service_context_loaded;
  iface->stop = ide_code_index_service_stop;
}

static void
ide_code_index_service_init (IdeCodeIndexService *self)
{
}

IdeCodeIndexIndex *
ide_code_index_service_get_index (IdeCodeIndexService *self)
{
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (self), NULL);

  return self->index;
}

// static IdeCodeIndexer *
// ide_code_index_service_real_get_code_indexer (IdeCodeIndexService *self,
//                                               PeasEngine          *engine,
//                                               GType                type,
//                                               const gchar         *key,
//                                               const gchar         *value)
// {
//   g_autoptr(IdeCodeIndexer) code_indexer = NULL;
//   g_auto(GStrv) plugins = NULL;
//   gint max_priority = -100;

//   g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
//   g_assert (value != NULL);

//   plugins = peas_engine_get_loaded_plugins (engine);

//   for (guint i = 0; plugins[i] != NULL; i++)
//     {
//       PeasPluginInfo *plugin_info;
//       g_autofree gchar *path = NULL;
//       g_autofree gchar *priority_key = NULL;
//       g_autoptr(GSettings) settings = NULL;
//       const gchar *values;
//       g_auto(GStrv) values_array = NULL;
//       const gchar *priority_value;
//       gint priority;

//       plugin_info = peas_engine_get_plugin_info (engine, plugins[i]);

//       path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/%s/",
//                               peas_plugin_info_get_module_name (plugin_info),
//                               g_type_name (type));

//       settings = g_settings_new_with_path ("org.gnome.builder.extension-type", path);

//       if (!g_settings_get_boolean (settings, "enabled"))
//         continue;

//       if (!peas_engine_provides_extension (engine, plugin_info, type))
//         continue;

//       values = peas_plugin_info_get_external_data (plugin_info, key);

//       if (values == NULL)
//         continue;

//       values_array = g_strsplit (values, ",", 0);

//       if (!g_strv_contains ((const gchar * const *)values_array, value))
//         continue;

//       priority_key = g_strconcat (key, "-Priority", NULL);
//       priority_value = peas_plugin_info_get_external_data (plugin_info, priority_key);
//       priority = (priority_value != NULL) ? atoi (priority_value) : 0;

//       if (priority > max_priority)
//         {
//           PeasExtension *extension;

//           max_priority = priority;
//           g_clear_object (&code_indexer);

//           extension = peas_engine_create_extension (engine,
//                                                     plugin_info,
//                                                     type,
//                                                     "context",
//                                                     ide_object_get_context (IDE_OBJECT (self)),
//                                                     NULL);

//           code_indexer = IDE_CODE_INDEXER (extension);
//         }
//     }

//   return g_steal_pointer (&code_indexer);
// }

IdeCodeIndexer *
ide_code_index_service_get_code_indexer (IdeCodeIndexService *self,
                                         const gchar         *file_name)
{
  IdeExtensionAdapter *code_indexer;
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *language;
  const gchar *lang;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (self), NULL);
  g_return_val_if_fail (file_name != NULL, NULL);

  if (self->code_indexers == NULL)
    return NULL;

  manager = gtk_source_language_manager_get_default ();
  language = gtk_source_language_manager_guess_language (manager, file_name, NULL);

  if (language == NULL)
    return NULL;

  lang = gtk_source_language_get_id (language);

  // if (!g_hash_table_lookup_extended (self->code_indexers, lang,
  //                                    NULL, (gpointer *)&code_indexer))
  //   {
  //     // code_indexer = ide_code_index_service_real_get_code_indexer (self,
  //     //                                                              peas_engine_get_default (),
  //     //                                                              IDE_TYPE_CODE_INDEXER,
  //     //                                                              "Code-Indexer-Languages",
  //     //                                                              lang);

  //     g_message ("%s", lang);

  //     code_indexer = ide_extension_adapter_new (ide_object_get_context (IDE_OBJECT (self)),
  //                                               NULL,
  //                                               IDE_TYPE_CODE_INDEXER,
  //                                               "Code-Indexer-Languages",
  //                                               lang);
  //     if (ide_extension_adapter_get_extension (code_indexer) == NULL)
  //       g_clear_object (&code_indexer);
  //     g_hash_table_insert (self->code_indexers, g_strdup (lang), code_indexer);
  //   }

  // if (code_indexer == NULL)
  //   return NULL;
  // else
  //   return ide_extension_adapter_get_extension (code_indexer);

  code_indexer = g_hash_table_lookup (self->code_indexers, lang);

  if (code_indexer == NULL)
    return NULL;
  else
    return ide_extension_adapter_get_extension (code_indexer);
}
