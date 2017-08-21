/* ide-clang-code-index-entry.c
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

#define G_LOG_DOMAIN "ide-clang-code-index-entry"

#include "ide-clang-code-index-entry.h"

/*
 * This class represents an index entry. We can set entry values using
 * ide_clang_code_index_entry_set function. This is used to deliver index
 * item to user of IdeClangCodeIndexer.
 */

struct _IdeClangCodeIndexEntry
{
  GObject parent;

  gchar          *name;
  gchar          *usr;
  IdeSymbolKind   kind;
  IdeSymbolFlags  flags;
  guint           begin_line;
  guint           begin_line_offset;
  guint           end_line;
  guint           end_line_offset;
};

static void code_index_entry_iface_init (IdeCodeIndexEntryInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangCodeIndexEntry, ide_clang_code_index_entry, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEX_ENTRY, code_index_entry_iface_init))

static gchar*
ide_clang_code_index_entry_get_name (IdeCodeIndexEntry *entry)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)entry;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self), NULL);

  return self->name;
}

static gchar*
ide_clang_code_index_entry_get_usr (IdeCodeIndexEntry *entry)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)entry;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self), NULL);

  return self->usr;
}

static IdeSymbolKind
ide_clang_code_index_entry_get_kind (IdeCodeIndexEntry *entry)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)entry;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self), IDE_SYMBOL_NONE);

  return self->kind;
}

static IdeSymbolFlags
ide_clang_code_index_entry_get_flags (IdeCodeIndexEntry *entry)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)entry;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self), IDE_SYMBOL_FLAGS_NONE);

  return self->flags;
}

static void
ide_clang_code_index_entry_get_range (IdeCodeIndexEntry *entry,
                                      guint             *begin_line,
                                      guint             *begin_line_offset,
                                      guint             *end_line,
                                      guint             *end_line_offset)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)entry;

  g_return_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self));

  if (begin_line != NULL)
    *begin_line = self->begin_line;
  if (begin_line_offset != NULL)
    *begin_line_offset = self->begin_line_offset;
  if (end_line != NULL)
    *end_line = self->end_line;
  if (end_line_offset != NULL)
    *end_line_offset = self->end_line_offset;
}

void
ide_clang_code_index_entry_set (IdeClangCodeIndexEntry *self,
                                const gchar            *name,
                                const gchar            *usr,
                                IdeSymbolKind           kind,
                                IdeSymbolFlags          flags,
                                guint                   begin_line,
                                guint                   begin_line_offset,
                                guint                   end_line,
                                guint                   end_line_offset)
{
  g_return_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRY (self));
  g_return_if_fail (name != NULL);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->usr, g_free);

  self->name = g_strdup (name);
  if (usr != NULL)
    self->usr = g_strdup (usr);
  self->kind = kind;
  self->flags = flags;
  self->begin_line = begin_line;
  self->begin_line_offset = begin_line_offset;
  self->end_line = end_line;
  self->end_line_offset = end_line_offset;
}

static void
ide_clang_code_index_entry_finalize (GObject *object)
{
  IdeClangCodeIndexEntry *self = (IdeClangCodeIndexEntry *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->usr, g_free);

  G_OBJECT_CLASS (ide_clang_code_index_entry_parent_class)->finalize (object);
}

static void
ide_clang_code_index_entry_class_init (IdeClangCodeIndexEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_clang_code_index_entry_finalize;
}

static void
code_index_entry_iface_init (IdeCodeIndexEntryInterface *iface)
{
  iface->get_name = ide_clang_code_index_entry_get_name;
  iface->get_usr = ide_clang_code_index_entry_get_usr;
  iface->get_kind = ide_clang_code_index_entry_get_kind;
  iface->get_flags = ide_clang_code_index_entry_get_flags;
  iface->get_range = ide_clang_code_index_entry_get_range;
}

static void
ide_clang_code_index_entry_init (IdeClangCodeIndexEntry *self)
{
}

IdeClangCodeIndexEntry*
ide_clang_code_index_entry_new ()
{
  return g_object_new (IDE_TYPE_CLANG_CODE_INDEX_ENTRY, NULL);
}
