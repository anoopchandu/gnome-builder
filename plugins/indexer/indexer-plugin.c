#include <libpeas/peas.h>
#include <ide.h>

#include "ide-indexer-service.h"

void
peas_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SERVICE,
                                              IDE_TYPE_INDEXER_SERVICE);
}