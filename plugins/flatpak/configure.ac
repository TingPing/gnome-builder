m4_define([flatpak_required_version], [0.6.9])

# --enable-flatpak-plugin=yes/no/auto
AC_ARG_ENABLE([flatpak-plugin],
              [AS_HELP_STRING([--enable-flatpak-plugin=@<:@yes/no/auto@:>@],
                              [Build with support for flatpak.])],
              [enable_flatpak_plugin=$enableval],
              [enable_flatpak_plugin=auto])

AS_IF([test "$enable_flatpak_plugin" != no],[
	PKG_CHECK_MODULES(FLATPAK,
	                  [flatpak >= flatpak_required_version
	                   json-glib-1.0 >= json_glib_required_version],
	                  [have_flatpak=yes],
	                  [have_flatpak=no])

       AS_IF([test "$enable_flatpak_plugin" = "yes" && test "$have_flatpak" = "no"],[
	       AC_MSG_ERROR([--enable-flatpak-plugin requires flatpak >= flatpak_required_version])
	])

	enable_flatpak_plugin=$have_flatpak
])

AM_CONDITIONAL(ENABLE_FLATPAK_PLUGIN, [test "x$enable_flatpak_plugin" = "xyes"])
AC_CONFIG_FILES([plugins/flatpak/Makefile])
