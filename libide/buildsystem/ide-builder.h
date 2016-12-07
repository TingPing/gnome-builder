/* ide-builder.h
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

#ifndef IDE_BUILDER_H
#define IDE_BUILDER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILDER (ide_builder_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuilder, ide_builder, IDE, BUILDER, IdeObject)

typedef enum
{
  IDE_BUILDER_BUILD_FLAGS_NONE            = 0,
  IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP = 1 << 0,
  IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN     = 1 << 1,
  IDE_BUILDER_BUILD_FLAGS_NO_BUILD        = 1 << 2,
  IDE_BUILDER_BUILD_FLAGS_NO_CONFIGURE    = 1 << 3,
} IdeBuilderBuildFlags;

struct _IdeBuilderClass
{
  IdeObjectClass parent;

  void             (*build_async)              (IdeBuilder            *self,
                                                IdeBuilderBuildFlags   flags,
                                                IdeBuildResult       **result,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data);
  IdeBuildResult  *(*build_finish)             (IdeBuilder            *self,
                                                GAsyncResult          *result,
                                                GError               **error);
  void             (*install_async)            (IdeBuilder            *self,
                                                IdeBuildResult       **result,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data);
  IdeBuildResult  *(*install_finish)           (IdeBuilder            *self,
                                                GAsyncResult          *result,
                                                GError               **error);
  void             (*get_build_flags_async)    (IdeBuilder            *self,
                                                IdeFile               *file,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data);
  gchar          **(*get_build_flags_finish)   (IdeBuilder            *self,
                                                GAsyncResult          *result,
                                                GError               **error);
  void             (*get_build_targets_async)  (IdeBuilder            *self,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data);
  GPtrArray       *(*get_build_targets_finish) (IdeBuilder            *self,
                                                GAsyncResult          *result,
                                                GError               **error);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

IdeConfiguration *ide_builder_get_configuration        (IdeBuilder            *self);
void              ide_builder_build_async              (IdeBuilder            *self,
                                                        IdeBuilderBuildFlags   flags,
                                                        IdeBuildResult       **result,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
IdeBuildResult   *ide_builder_build_finish             (IdeBuilder            *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
void              ide_builder_install_async            (IdeBuilder            *self,
                                                        IdeBuildResult       **result,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
IdeBuildResult   *ide_builder_install_finish           (IdeBuilder            *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
void              ide_builder_get_build_targets_async  (IdeBuilder            *self,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
GPtrArray        *ide_builder_get_build_targets_finish (IdeBuilder            *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
void              ide_builder_get_build_flags_async    (IdeBuilder            *self,
                                                        IdeFile               *file,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
gchar           **ide_builder_get_build_flags_finish   (IdeBuilder            *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);

G_END_DECLS

#endif /* IDE_BUILDER_H */
