# --enable-mingw-plugin=yes/no
AC_ARG_ENABLE([mingw-plugin],
              [AS_HELP_STRING([--enable-mingw-plugin=@<:@yes/no@:>@],
                              [Build with support for the mingw compiler.])],
              [enable_mingw_plugin=$enableval],
              [enable_mingw_plugin=yes])

# for if ENABLE_MINGW_PLUGIN in Makefile.am
AM_CONDITIONAL(ENABLE_MINGW_PLUGIN, test x$enable_mingw_plugin != xno)

# Ensure our makefile is generated by autoconf
AC_CONFIG_FILES([plugins/mingw/Makefile])
