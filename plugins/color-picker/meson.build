if get_option('with_color_picker')

color_picker_resources = gnome.compile_resources(
  'gb-color-picker-resources',
  'gb-color-picker.gresource.xml',
  c_name: 'gb_color_picker',
)

color_picker_sources = [
  'gb-color-picker-helper.c',
  'gb-color-picker-helper.h',
  'gb-color-picker-plugin.c',
  'gb-color-picker-document-monitor.c',
  'gb-color-picker-document-monitor.h',
  'gb-color-picker-prefs.c',
  'gb-color-picker-prefs.h',
  'gb-color-picker-prefs-list.c',
  'gb-color-picker-prefs-palette-list.h',
  'gb-color-picker-prefs-palette-list.c',
  'gb-color-picker-prefs-list.h',
  'gb-color-picker-prefs-palette-row.c',
  'gb-color-picker-prefs-palette-row.h',
  'gb-color-picker-private.h',
  'gb-color-picker-workbench-addin.c',
  'gb-color-picker-workbench-addin.h',
  'gb-color-picker-workbench-addin-private.h',
  color_picker_resources[0],
]

color_picker_deps = plugin_deps + [
  libgstyle_dep,
]

shared_module('color-picker-plugin', color_picker_sources,
  dependencies: color_picker_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('color-picker.plugin', install_dir: plugindir)
install_data('gsettings/org.gnome.builder.plugins.color_picker_plugin.gschema.xml',
  install_dir: schema_dir,
)

endif
