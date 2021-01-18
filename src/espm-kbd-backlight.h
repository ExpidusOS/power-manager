/*
 * * Copyright (C) 2013 Sonal Santan <sonal.santan@gmail.com>
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

#ifndef __ESPM_KBD_BACKLIGHT_H
#define __ESPM_KBD_BACKLIGHT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_KBD_BACKLIGHT        (espm_kbd_backlight_get_type () )
#define ESPM_KBD_BACKLIGHT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_KBD_BACKLIGHT, EspmKbdBacklight))
#define ESPM_IS_KBD_BACKLIGHT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_KBD_BACKLIGHT))

typedef struct EspmKbdBacklightPrivate EspmKbdBacklightPrivate;

typedef struct
{
  GObject                     parent;
  EspmKbdBacklightPrivate    *priv;

} EspmKbdBacklight;

typedef struct
{
  GObjectClass                parent_class;

} EspmKbdBacklightClass;

GType                           espm_kbd_backlight_get_type         (void) G_GNUC_CONST;
EspmKbdBacklight               *espm_kbd_backlight_new              (void);
gboolean                        espm_kbd_backlight_has_hw           (EspmKbdBacklight *backlight);

G_END_DECLS

#endif /* __ESPM_KBD_BACKLIGHT_H */
