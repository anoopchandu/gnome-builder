/* ide-code-index-search-provider.c
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

#define G_LOG_DOMAIN "ide-code-index-search-provider"

#include "ide-code-index-search-provider.h"
#include "ide-code-index-service.h"
#include "ide-code-index-index.h"

struct _IdeCodeIndexSearchProvider
{
  IdeObject parent;
};

static void search_provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeCodeIndexSearchProvider, ide_code_index_search_provider, IDE_TYPE_OBJECT,
                        0, G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
populate_cb (GObject           *object,
             GAsyncResult      *result,
             gpointer           user_data)
{
  IdeCodeIndexIndex *index = (IdeCodeIndexIndex *)object;
  g_autoptr(GTask) task = user_data;
  GPtrArray *results;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CODE_INDEX_INDEX (index));
  g_assert (G_IS_TASK (task));

  if (NULL != (results = ide_code_index_index_populate_finish (index, result, &error)))
    g_task_return_pointer (task, results, (GDestroyNotify)g_ptr_array_unref);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
ide_code_index_search_provider_search_async (IdeSearchProvider   *provider,
                                             const gchar         *search_terms,
                                             guint                max_results,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeCodeIndexSearchProvider *self = (IdeCodeIndexSearchProvider *)provider;
  IdeCodeIndexService *service;
  IdeCodeIndexIndex *index;
  GTask *task;

  g_return_if_fail (IDE_IS_CODE_INDEX_SEARCH_PROVIDER (self));

  service = ide_context_get_service_typed (ide_object_get_context (IDE_OBJECT (self)),
                                           IDE_TYPE_CODE_INDEX_SERVICE);
  index = ide_code_index_service_get_index (service);

  task = g_task_new (self, cancellable, callback, user_data);

  ide_code_index_index_populate_async (index, search_terms, max_results, cancellable,
                                       populate_cb, g_steal_pointer (&task));
}

static GPtrArray*
ide_code_index_search_provider_search_finish (IdeSearchProvider *provider,
                                              GAsyncResult      *result,
                                              GError           **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK(task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = ide_code_index_search_provider_search_async;
  iface->search_finish = ide_code_index_search_provider_search_finish;
}

static void
ide_code_index_search_provider_init (IdeCodeIndexSearchProvider *self)
{
}

static void
ide_code_index_search_provider_class_init (IdeCodeIndexSearchProviderClass *klass)
{
}