/* ide-code-index-entry.c
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

#define G_LOG_DOMAIN "ide-code-index-entry"

#include "ide-code-index-entry.h"

G_DEFINE_INTERFACE (IdeCodeIndexEntry, ide_code_index_entry, G_TYPE_OBJECT)

static void
ide_code_index_entry_default_init (IdeCodeIndexEntryInterface *iface)
{
}

gchar*
ide_code_index_entry_get_usr (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), NULL);

  iface = IDE_CODE_INDEX_ENTRY_GET_IFACE (self);

  if (iface->get_usr == NULL)
    return NULL;

  return iface->get_usr (self);
}

gchar*
ide_code_index_entry_get_name (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), NULL);

  iface = IDE_CODE_INDEX_ENTRY_GET_IFACE (self);

  if (iface->get_name == NULL)
    return NULL;

  return iface->get_name (self);
}

IdeSymbolKind
ide_code_index_entry_get_kind (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), IDE_SYMBOL_NONE);

  iface = IDE_CODE_INDEX_ENTRY_GET_IFACE (self);

  if (iface->get_kind == NULL)
    return IDE_SYMBOL_NONE;

  return iface->get_kind (self);
}

IdeSymbolFlags
ide_code_index_entry_get_flags (IdeCodeIndexEntry *self)
{
  IdeCodeIndexEntryInterface *iface;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRY (self), IDE_SYMBOL_FLAGS_NONE);

  iface = IDE_CODE_INDEX_ENTRY_GET_IFACE (self);

  if (iface->get_flags == NULL)
    return IDE_SYMBOL_FLAGS_NONE;

  return iface->get_flags (self);
}

void
ide_code_index_entry_get_range (IdeCodeIndexEntry *self,
                                guint             *begin_line,
                                guint             *begin_line_offset,
                                guint             *end_line,
                                guint             *end_line_offset)
{
  IdeCodeIndexEntryInterface *iface;

  g_return_if_fail (IDE_IS_CODE_INDEX_ENTRY (self));

  iface = IDE_CODE_INDEX_ENTRY_GET_IFACE (self);

  if (iface->get_range == NULL)
    {
      begin_line = begin_line_offset = end_line = end_line_offset = NULL;
      return;
    }

  iface->get_range (self,
                    begin_line,
                    begin_line_offset,
                    end_line,
                    end_line_offset);
}
