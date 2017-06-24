if get_option('with_indexer')

indexer_sources = [
  'ide-indexer-index.c',
  'ide-indexer-index.h',
  'ide-indexer-service.c',
  'ide-indexer-service.h',
  'ide-indexer-builder.c',
  'ide-indexer-builder.h',
  'tool/ide-simple-table.c',
  'tool/ide-simple-table.h',
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

subdir ('tool')

endif