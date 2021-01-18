/*
 * * Copyright (C) 2009-2011 Ali <aliov@expidus.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __ESPM_CONSOLE_KIT_H
#define __ESPM_CONSOLE_KIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_CONSOLE_KIT        (espm_console_kit_get_type () )
#define ESPM_CONSOLE_KIT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_CONSOLE_KIT, EspmConsoleKit))
#define ESPM_IS_CONSOLE_KIT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_CONSOLE_KIT))

typedef struct EspmConsoleKitPrivate EspmConsoleKitPrivate;

typedef struct
{
  GObject                     parent;
  EspmConsoleKitPrivate      *priv;
} EspmConsoleKit;

typedef struct
{
  GObjectClass                parent_class;
} EspmConsoleKitClass;

GType              espm_console_kit_get_type        (void) G_GNUC_CONST;
EspmConsoleKit    *espm_console_kit_new             (void);
void               espm_console_kit_shutdown        (EspmConsoleKit *console,
                                                     GError **error);
void               espm_console_kit_reboot          (EspmConsoleKit *console,
                                                     GError **error);
void               espm_console_kit_suspend         (EspmConsoleKit *console,
                                                     GError        **error);
void               espm_console_kit_hibernate       (EspmConsoleKit *console,
                                                     GError        **error);

G_END_DECLS

#endif /* __ESPM_CONSOLE_KIT_H */
