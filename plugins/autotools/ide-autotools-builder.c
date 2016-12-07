/* ide-autotools-builder.c
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

#define G_LOG_DOMAIN "ide-autotools-builder"

#include <egg-counter.h>
#include <egg-task-cache.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "ide-autotools-build-task.h"
#include "ide-autotools-builder.h"
#include "ide-makecache.h"

#define DEFAULT_MAKECACHE_TTL_MSECS (5 * 60 * 1000L)

EGG_DEFINE_COUNTER (build_flags, "Autotools", "Flags Requests", "Requests count for build flags")

struct _IdeAutotoolsBuilder
{
  IdeBuilder parent_instance;
};

G_DEFINE_TYPE (IdeAutotoolsBuilder, ide_autotools_builder, IDE_TYPE_BUILDER)

static EggTaskCache *makecaches;

static void
get_makecache_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  EggTaskCache *cache = (EggTaskCache *)object;
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  g_assert (EGG_IS_TASK_CACHE (cache));
  g_assert (G_IS_ASYNC_RESULT (result));

  makecache = egg_task_cache_get_finish (cache, result, &error);

  if (makecache == NULL)
    {
      g_task_return_error (G_TASK (task), g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_steal_pointer (&makecache), g_object_unref);

  IDE_EXIT;
}

static void
get_makecache_async (IdeConfiguration    *configuration,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, get_makecache_async);

  egg_task_cache_get_async (makecaches,
                            configuration,
                            FALSE,
                            cancellable,
                            get_makecache_cb,
                            g_steal_pointer (&task));
}

static IdeMakecache *
get_makecache_finish (GAsyncResult  *result,
                      GError       **error)
{
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_autotools_builder_get_build_flags_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  g_assert (object == NULL);
  g_assert (G_IS_ASYNC_RESULT (result));

  EGG_COUNTER_DEC (build_flags);

  makecache = get_makecache_finish (result, &error);

  if (makecache == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_steal_pointer (&makecache), g_object_unref);

  IDE_EXIT;
}

static void
ide_autotools_builder_get_build_flags_async (IdeBuilder          *builder,
                                             IdeFile             *file,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)builder;
  g_autoptr(GTask) task = NULL;
  IdeConfiguration *configuration;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (self));
  g_assert (IDE_IS_FILE (file));

  EGG_COUNTER_INC (build_flags);

  gfile = ide_file_get_file (file);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_builder_get_build_flags_async);
  g_task_set_task_data (task, g_object_ref (gfile), g_object_unref);

  configuration = ide_builder_get_configuration (builder);
  g_assert (IDE_IS_CONFIGURATION (configuration));

  get_makecache_async (configuration,
                       cancellable,
                       ide_autotools_builder_get_build_flags_cb,
                       g_steal_pointer (&task));

  IDE_EXIT;
}

static gchar **
ide_autotools_builder_get_build_flags_finish (IdeBuilder    *builder,
                                              GAsyncResult  *result,
                                              GError       **error)
{
  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}


static void
ide_autotools_builder_build_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeAutotoolsBuildTask *build_result = (IdeAutotoolsBuildTask *)object;
  GError *error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (build_result));
  g_assert (G_IS_TASK (task));

  if (!ide_autotools_build_task_execute_with_postbuild_finish (build_result, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Build cancelled"));
      else
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Build failed"));

      g_task_return_error (task, error);
      return;
    }

  ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Build successful"));

  g_task_return_pointer (task, g_object_ref (build_result), g_object_unref);
}

/**
 * ide_autotools_builder_get_build_directory:
 *
 * Gets the directory that will contain the generated makefiles and build root.
 *
 * Returns: (transfer full): A #GFile containing the build directory.
 */
GFile *
ide_autotools_builder_get_build_directory (IdeAutotoolsBuilder *self)
{
  g_autofree gchar *path = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeProject *project;
  IdeDevice *device;
  const gchar *root_build_dir;
  const gchar *project_id;
  const gchar *device_id;
  const gchar *system_type;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  configuration = ide_builder_get_configuration (IDE_BUILDER (self));

  device = ide_configuration_get_device (configuration);
  device_id = ide_device_get_id (device);

  /*
   * If this is the local device, we have a special workaround for building within the project
   * tree. Generally we want to be doing out of tree builds, but a lot of people are going to
   * fire up their project from jhbuild or similar, and build in tree.
   *
   * This workaround will let us continue building their project in that location, with the
   * caveat that we will need to `make distclean` later if they want to build for another device.
   */
  if (0 == g_strcmp0 (device_id, "local"))
    {
      IdeVcs *vcs;
      GFile *working_directory;
      g_autoptr(GFile) makefile_file = NULL;
      g_autofree gchar *makefile_path = NULL;

      vcs = ide_context_get_vcs (context);
      working_directory = ide_vcs_get_working_directory (vcs);
      makefile_file = g_file_get_child (working_directory, "Makefile");
      makefile_path = g_file_get_path (makefile_file);

      /*
       * NOTE:
       *
       * It would be nice if this was done asynchronously, but if this isn't fast, we will have
       * stalled in so many other places that the app will probably be generally unusable. So
       * I'm going to cheat for now and make this function synchronous.
       */
      if (g_file_test (makefile_path, G_FILE_TEST_EXISTS))
        return g_object_ref (working_directory);
    }

  project = ide_context_get_project (context);
  root_build_dir = ide_context_get_root_build_dir (context);
  system_type = ide_device_get_system_type (device);
  project_id = ide_project_get_id (project);
  path = g_build_filename (root_build_dir, project_id, device_id, system_type, NULL);

  return g_file_new_for_path (path);
}

static void
ide_autotools_builder_build_async (IdeBuilder           *builder,
                                   IdeBuilderBuildFlags  flags,
                                   IdeBuildResult      **result,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)builder;
  g_autoptr(IdeAutotoolsBuildTask) build_result = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) directory = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (IDE_IS_AUTOTOOLS_BUILDER (self));

  if (ide_autotools_builder_get_needs_bootstrap (self))
    flags |= IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP;

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (builder));
  configuration = ide_builder_get_configuration (IDE_BUILDER (self));
  directory = ide_autotools_builder_get_build_directory (self);
  build_result = g_object_new (IDE_TYPE_AUTOTOOLS_BUILD_TASK,
                               "context", context,
                               "configuration", configuration,
                               "directory", directory,
                               "mode", _("Building…"),
                               "running", TRUE,
                               "install", FALSE,
                               NULL);

  if (result != NULL)
    *result = g_object_ref (build_result);

  ide_autotools_build_task_execute_with_postbuild (build_result,
                                                   flags,
                                                   cancellable,
                                                   ide_autotools_builder_build_cb,
                                                   g_object_ref (task));
}

static IdeBuildResult *
ide_autotools_builder_build_finish (IdeBuilder    *builder,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

#if 0

static void
ide_autotools_build_system_get_build_targets_cb2 (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IdeMakecache *makecache = (IdeMakecache *)object;
  g_autoptr(GTask) task = user_data;
  GPtrArray *ret;
  GError *error = NULL;

  g_assert (IDE_IS_MAKECACHE (makecache));
  g_assert (G_IS_TASK (task));

  ret = ide_makecache_get_build_targets_finish (makecache, result, &error);

  if (ret == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, ret, (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_autotools_build_system_get_build_targets_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)object;
  IdeContext *context;
  IdeVcs *vcs;
  g_autoptr(IdeConfiguration) configuration = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GFile) build_dir = NULL;
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (G_IS_TASK (task));

  makecache = ide_autotools_build_system_get_makecache_finish (self, result, &error);

  if (makecache == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  configuration = ide_configuration_new (context, "autotools-bootstrap", "local", "host");
  builder = ide_autotools_build_system_get_builder (IDE_BUILD_SYSTEM (self), configuration, &error);
  if (builder)
    {
      build_dir = ide_autotools_builder_get_build_directory (IDE_AUTOTOOLS_BUILDER (builder));
    }
  else
    {
      vcs = ide_context_get_vcs (context);
      build_dir = ide_vcs_get_working_directory (vcs);
    }

  ide_makecache_get_build_targets_async (makecache,
                                         build_dir,
                                         g_task_get_cancellable (task),
                                         ide_autotools_build_system_get_build_targets_cb2,
                                         g_object_ref (task));
}

static void
ide_autotools_build_system_get_build_targets_async (IdeBuildSystem      *build_system,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)build_system;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_build_system_get_build_targets_async);

  ide_autotools_build_system_get_makecache_async (self,
                                                  cancellable,
                                                  ide_autotools_build_system_get_build_targets_cb,
                                                  g_object_ref (task));
}

static GPtrArray *
ide_autotools_build_system_get_build_targets_finish (IdeBuildSystem  *build_system,
                                                     GAsyncResult    *result,
                                                     GError         **error)
{
  IdeAutotoolsBuildSystem *self = (IdeAutotoolsBuildSystem *)build_system;
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (self));
  g_assert (G_IS_TASK (task));
  g_assert (g_task_is_valid (task, self));
  g_assert (g_task_get_source_tag (task) == ide_autotools_build_system_get_build_targets_async);

  return g_task_propagate_pointer (task, error);
}

#endif

static void
ide_autotools_builder_install_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeAutotoolsBuildTask *build_task = (IdeAutotoolsBuildTask *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TASK (build_task));
  g_assert (G_IS_TASK (task));

  if (!ide_autotools_build_task_execute_with_postbuild_finish (build_task, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_task), _("Install cancelled"));
      else
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_task), _("Install failed"));

      g_task_return_error (task, error);
      return;
    }

  ide_build_result_set_mode (IDE_BUILD_RESULT (build_task), _("Install successful"));

  g_task_return_pointer (task, g_object_ref (build_task), g_object_unref);
}

static void
ide_autotools_builder_install_async (IdeBuilder           *builder,
                                     IdeBuildResult      **result,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)builder;
  g_autoptr(IdeAutotoolsBuildTask) build_result = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) directory = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeBuilderBuildFlags flags;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (IDE_IS_AUTOTOOLS_BUILDER (self));

  flags = IDE_BUILDER_BUILD_FLAGS_NONE;
  if (ide_autotools_builder_get_needs_bootstrap (self))
    flags |= IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP;

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (builder));
  configuration = ide_builder_get_configuration (IDE_BUILDER (self));
  directory = ide_autotools_builder_get_build_directory (self);
  build_result = g_object_new (IDE_TYPE_AUTOTOOLS_BUILD_TASK,
                               "context", context,
                               "configuration", configuration,
                               "directory", directory,
                               "mode", _("Building…"),
                               "running", TRUE,
                               "install", TRUE,
                               NULL);

  ide_autotools_build_task_add_target (build_result, "install");

  if (result != NULL)
    *result = g_object_ref (build_result);

  ide_autotools_build_task_execute_with_postbuild (build_result,
                                                   flags,
                                                   cancellable,
                                                   ide_autotools_builder_install_cb,
                                                   g_object_ref (task));
}

static IdeBuildResult *
ide_autotools_builder_install_finish (IdeBuilder    *builder,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
new_makecache_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  makecache = ide_makecache_new_for_makefile_finish (result, &error);

  if (makecache == NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&makecache), g_object_unref);

  IDE_EXIT;
}

static void
ensure_makefile_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeAutotoolsBuilder *builder = (IdeAutotoolsBuilder *)object;
  g_autoptr(IdeBuildResult) build_resualt = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) build_directory = NULL;
  g_autoptr(GFile) makefile = NULL;
  g_autoptr(IdeBuildResult) build_result = NULL;
  IdeConfiguration *configuration;
  GCancellable *cancellable;
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  build_result = ide_builder_build_finish (IDE_BUILDER (builder), result, &error);

  if (build_result == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  configuration = ide_builder_get_configuration (IDE_BUILDER (builder));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  build_directory = ide_autotools_builder_get_build_directory (builder);
  g_assert (G_IS_FILE (build_directory));

  makefile = g_file_get_child (build_directory, "Makefile");
  g_assert (G_IS_FILE (makefile));

  runtime = ide_configuration_get_runtime (configuration);
  g_assert (!runtime || IDE_IS_RUNTIME (runtime));

  if (runtime == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to locate runtime “%s”",
                               ide_configuration_get_runtime_id (configuration));
      IDE_EXIT;
    }

  cancellable = g_task_get_cancellable (task);

  ide_makecache_new_for_makefile_async (runtime,
                                        makefile,
                                        cancellable,
                                        new_makecache_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

static void
populate_cache_cb (EggTaskCache  *cache,
                   gconstpointer  key,
                   GTask         *task,
                   gpointer       user_data)
{
  IdeConfiguration *configuration = (IdeConfiguration *)key;
  g_autoptr(IdeBuilder) builder = NULL;
  GCancellable *cancellable;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (G_IS_TASK (task));

  context = ide_object_get_context (IDE_OBJECT (configuration));
  g_assert (IDE_IS_CONTEXT (context));

  builder = g_object_new (IDE_TYPE_AUTOTOOLS_BUILDER,
                          "context", context,
                          "configuration", configuration,
                          NULL);

  cancellable = g_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Next we need to ensure that the project is at least boostrapped,
   * so that we can do a dry run on the makefile. So we just use an
   * additional IdeBuilder with an identical configuration so that
   * we have access to the Makefile/etc.
   *
   * Once that is complete, we can locate the build Makefile and then
   * run our make process (via makecache) in the target runtime.
   */
  ide_builder_build_async (builder,
                           IDE_BUILDER_BUILD_FLAGS_NO_BUILD,
                           NULL,
                           cancellable,
                           ensure_makefile_cb,
                           g_object_ref (task));

  IDE_EXIT;
}

static guint
config_hash (gconstpointer a)
{
  g_autofree gchar *key = NULL;
  IdeConfiguration *config = (IdeConfiguration *)a;

  g_assert (IDE_IS_CONFIGURATION (config));

  key = g_strdup_printf ("%s|%u",
                         ide_configuration_get_id (config),
                         ide_configuration_get_sequence (config));

  return g_str_hash (key);
}

static gboolean
config_equal (gconstpointer a,
              gconstpointer b)
{
  IdeConfiguration *config_a = (IdeConfiguration *)a;
  IdeConfiguration *config_b = (IdeConfiguration *)b;

  g_assert (IDE_IS_CONFIGURATION (config_a));
  g_assert (IDE_IS_CONFIGURATION (config_b));

  return (g_strcmp0 (ide_configuration_get_id (config_a), ide_configuration_get_id (config_b)) == 0) &&
         (ide_configuration_get_sequence (config_a) == ide_configuration_get_sequence (config_b));
}

static void
ide_autotools_builder_class_init (IdeAutotoolsBuilderClass *klass)
{
  IdeBuilderClass *builder_class = IDE_BUILDER_CLASS (klass);

  builder_class->build_async = ide_autotools_builder_build_async;
  builder_class->build_finish = ide_autotools_builder_build_finish;
  builder_class->install_async = ide_autotools_builder_install_async;
  builder_class->install_finish = ide_autotools_builder_install_finish;
  builder_class->get_build_flags_async = ide_autotools_builder_get_build_flags_async;
  builder_class->get_build_flags_finish = ide_autotools_builder_get_build_flags_finish;

  makecaches = egg_task_cache_new (config_hash,
                                   config_equal,
                                   g_object_ref,
                                   g_object_unref,
                                   g_object_ref,
                                   g_object_unref,
                                   DEFAULT_MAKECACHE_TTL_MSECS,
                                   populate_cache_cb,
                                   NULL,
                                   NULL);
  egg_task_cache_set_name (makecaches, "makecache");
}

static void
ide_autotools_builder_init (IdeAutotoolsBuilder *self)
{
}

gboolean
ide_autotools_builder_get_needs_bootstrap (IdeAutotoolsBuilder *self)
{
  g_autoptr(GFile) configure = NULL;
  GFile *working_directory = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeVcs *vcs;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self), FALSE);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  vcs = ide_context_get_vcs (context);
  working_directory = ide_vcs_get_working_directory (vcs);
  configure = g_file_get_child (working_directory, "configure");

  if (!g_file_query_exists (configure, NULL))
    return TRUE;

  configuration = ide_builder_get_configuration (IDE_BUILDER (self));
  if (ide_configuration_get_dirty (configuration))
    return TRUE;

  /*
   * TODO:
   *
   * We might also want to check for dependent files being out of date. For example, if autogen.sh
   * is newer than configure, we should bootstrap. Of course, once we go this far, I'd prefer
   * to make this function asynchronous.
   */

  return FALSE;
}
