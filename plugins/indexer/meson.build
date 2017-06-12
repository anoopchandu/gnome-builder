if get_option('with_indexer')

indexer_sources = [
  'ide-indexer-index-builder.c',
  'ide-indexer-index-builder.h',
  'ide-indexer-index.c',
  'ide-indexer-index.h',
  'ide-indexer-service.c',
  'ide-indexer-service.h',
  'indexer-plugin.c',
]

shared_module('indexer-plugin', indexer_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('indexer.plugin', install_dir: plugindir)

endif
