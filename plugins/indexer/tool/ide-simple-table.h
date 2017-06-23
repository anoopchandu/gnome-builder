#ifndef IDE_TYPE_SIMPLE_TABLE_H
#define IDE_TYPE_SIMPLE_TABLE_H

#include <glib-object.h>

#define IDE_TYPE_SIMPLE_TABLE (ide_simple_table_get_type ())

G_DECLARE_FINAL_TYPE (IdeSimpleTable, ide_simple_table, IDE, SIMPLE_TABLE, GObject)

gboolean        ide_simple_table_load_file   (IdeSimpleTable *self,
                                              GFile          *file,
                                              GCancellable   *cancellable,
                                              GError        **error);
gboolean        ide_simple_table_search      (IdeSimpleTable *self,
                                              const gchar    *key,
                                              gint          **values,
                                              gint           *num_values);
    IdeSimpleTable* ide_simple_table_new     (void);

#endif /* IDE_TYPE_SIMPLE_TABLE_H */