if ENABLE_TODO_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = todo.plugin

moduledir = $(libdir)/gnome-builder/plugins/todo_plugin
dist_module_DATA = todo_plugin/__init__.py

TAGS_FILES = $(dist_module_DATA)

endif

-include $(top_srcdir)/git.mk
