#include <glib-object.h>
#include <gio/gio.h>

#ifndef IDE_TYPE_SIMPLE_TABLE_BUILDER_H
#define IDE_TYPE_SIMPLE_TABLE_BUILDER_H

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_TABLE_BUILDER (ide_simple_table_builder_get_type ())

G_DECLARE_FINAL_TYPE (IdeSimpleTableBuilder, ide_simple_table_builder, IDE, SIMPLE_TABLE_BUILDER, GObject)

IdeSimpleTableBuilder*   ide_simple_table_builder_new    (gint32                  num_columns);
void                     ide_simple_table_builder_insert (IdeSimpleTableBuilder *self,
                                                          const gchar           *key,
                                                          gint                  *values);
gboolean                 ide_simple_table_builder_write  (IdeSimpleTableBuilder *self,
                                                          GFile                 *file,
                                                          GCancellable          *cancellable,
                                                          GError               **error);

G_END_DECLS

#endif /* IDE_TYPE_SIMPLE_TABLE_BUILDER_H */
