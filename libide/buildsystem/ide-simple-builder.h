/* ide-simple-builder.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_SIMPLE_BUILDER_H
#define IDE_SIMPLE_BUILDER_H

#include "buildsystem/ide-builder.h"
#include "buildsystem/ide-configuration.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_BUILDER (ide_simple_builder_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSimpleBuilder, ide_simple_builder, IDE, SIMPLE_BUILDER, IdeBuilder)

struct _IdeSimpleBuilderClass
{
  IdeBuilderClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeSimpleBuilder *ide_simple_builder_new (IdeConfiguration *configuration);

G_END_DECLS

#endif /* IDE_SIMPLE_BUILDER_H */
