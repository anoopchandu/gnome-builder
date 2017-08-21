/* ide-code-index-search-result.c
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

#define G_LOG_DOMAIN "ide-code-index-search-result"

#include "ide-code-index-search-result.h"

struct _IdeCodeIndexSearchResult
{
  IdeSearchResult    parent;

  IdeContext        *context;
  IdeSourceLocation *location;
};

G_DEFINE_TYPE (IdeCodeIndexSearchResult, ide_code_index_search_result, IDE_TYPE_SEARCH_RESULT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_LOCATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static IdeSourceLocation *
ide_code_index_search_result_get_source_location (IdeSearchResult *result)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)result;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_SEARCH_RESULT (self), NULL);

  return ide_source_location_ref (self->location);
}

static void
ide_code_index_search_result_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_LOCATION:
      g_value_set_pointer (value, self->location);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_search_result_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_set_weak_pointer (&self->context, g_value_get_object (value));
      break;

    case PROP_LOCATION:
      self->location = ide_source_location_ref (g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_search_result_finalize (GObject *object)
{
  IdeCodeIndexSearchResult *self = (IdeCodeIndexSearchResult *)object;

  ide_clear_weak_pointer (&self->context);
  g_clear_pointer (&self->location, ide_source_location_unref);

  G_OBJECT_CLASS (ide_code_index_search_result_parent_class)->finalize (object);
}

static void
ide_code_index_search_result_class_init (IdeCodeIndexSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->get_property = ide_code_index_search_result_get_property;
  object_class->set_property = ide_code_index_search_result_set_property;
  object_class->finalize = ide_code_index_search_result_finalize;

  result_class->get_source_location = ide_code_index_search_result_get_source_location;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context for the result",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCATION] =
    g_param_spec_pointer ("location",
                          "location",
                          "Location of symbol.",
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_code_index_search_result_init (IdeCodeIndexSearchResult *self)
{
}

IdeCodeIndexSearchResult*
ide_code_index_search_result_new (IdeContext        *context,
                                  const gchar       *title,
                                  const gchar       *icon_name,
                                  IdeSourceLocation *location,
                                  gfloat             score)
{
  return g_object_new (IDE_TYPE_CODE_INDEX_SEARCH_RESULT,
                       "context", context,
                       "title", title,
                       "icon-name", icon_name,
                       "location", location,
                       "score", score,
                       NULL);
}
