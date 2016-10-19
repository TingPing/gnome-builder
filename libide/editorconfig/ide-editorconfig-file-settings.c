/* ide-editorconfig-file-settings.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editorconfig"

#include "editorconfig-glib.h"
#include <glib/gi18n.h>

#include "ide-debug.h"

#include "editorconfig/ide-editorconfig-file-settings.h"
#include "files/ide-file.h"

struct _IdeEditorconfigFileSettings
{
  IdeFileSettings parent_instance;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEditorconfigFileSettings,
                        ide_editorconfig_file_settings,
                        IDE_TYPE_FILE_SETTINGS,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

static void
ide_editorconfig_file_settings_class_init (IdeEditorconfigFileSettingsClass *klass)
{
}

static void
ide_editorconfig_file_settings_init (IdeEditorconfigFileSettings *self)
{
}

static void
ide_editorconfig_file_settings_init_worker (GTask        *task,
                                            gpointer      source_object,
                                            gpointer      task_data,
                                            GCancellable *cancellable)
{
  GFile *file = task_data;
  GHashTableIter iter;
  GHashTable *ht;
  gpointer k, v;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_EDITORCONFIG_FILE_SETTINGS (source_object));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ht = editorconfig_glib_read (file, cancellable, &error);

  if (!ht)
    {
      g_task_return_error (task, error);
      return;
    }

  g_hash_table_iter_init (&iter, ht);

  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      const gchar *key = k;
      const GValue *value = v;

      if (g_str_equal (key, "indent_size"))
        g_object_set_property (source_object, "indent-width", value);
      else if (g_str_equal (key, "tab_width") ||
               g_str_equal (key, "trim_trailing_whitespace"))
        g_object_set_property (source_object, key, value);
      else if (g_str_equal (key, "insert_final_newline"))
        g_object_set_property (source_object, "insert-trailing-newline", value);
      else if (g_str_equal (key, "charset"))
        g_object_set_property (source_object, "encoding", value);
      else if (g_str_equal (key, "max_line_length"))
        {
          g_object_set_property (source_object, "right-margin-position", value);
          g_object_set (source_object, "show-right-margin", TRUE, NULL);
        }
      else if (g_str_equal (key, "end_of_line"))
        {
          GtkSourceNewlineType newline_type = GTK_SOURCE_NEWLINE_TYPE_LF;
          const gchar *str;

          str = g_value_get_string (value);
          if (g_strcmp0 (str, "cr") == 0)
            newline_type = GTK_SOURCE_NEWLINE_TYPE_CR;
          else if (g_strcmp0 (str, "crlf") == 0)
            newline_type = GTK_SOURCE_NEWLINE_TYPE_CR_LF;

          ide_file_settings_set_newline_type (source_object, newline_type);
        }
      else if (g_str_equal (key, "indent_style"))
        {
          IdeIndentStyle indent_style = IDE_INDENT_STYLE_SPACES;
          const gchar *str;

          str = g_value_get_string (value);

          if (g_strcmp0 (str, "tab") == 0)
            indent_style = IDE_INDENT_STYLE_TABS;

          ide_file_settings_set_indent_style (source_object, indent_style);
        }
    }

  g_task_return_boolean (task, TRUE);

  g_hash_table_unref (ht);
}

static void
ide_editorconfig_file_settings_init_async (GAsyncInitable      *initable,
                                           gint                 io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeEditorconfigFileSettings *self = (IdeEditorconfigFileSettings *)initable;
  g_autoptr(GTask) task = NULL;
  IdeFile *file;
  GFile *gfile = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITORCONFIG_FILE_SETTINGS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  file = ide_file_settings_get_file (IDE_FILE_SETTINGS (self));
  if (file)
    gfile = ide_file_get_file (file);

  if (!gfile)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("No file was provided."));
      IDE_EXIT;
    }

  g_task_set_task_data (task, g_object_ref (gfile), g_object_unref);
  g_task_run_in_thread (task, ide_editorconfig_file_settings_init_worker);

  IDE_EXIT;
}

static gboolean
ide_editorconfig_file_settings_init_finish (GAsyncInitable  *initable,
                                            GAsyncResult    *result,
                                            GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_editorconfig_file_settings_init_async;
  iface->init_finish = ide_editorconfig_file_settings_init_finish;
}
