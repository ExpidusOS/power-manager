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

#ifndef __ESPM_BACKLIGHT_H
#define __ESPM_BACKLIGHT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_BACKLIGHT        (espm_backlight_get_type () )
#define ESPM_BACKLIGHT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_BACKLIGHT, EspmBacklight))
#define ESPM_IS_BACKLIGHT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_BACKLIGHT))

typedef struct EspmBacklightPrivate EspmBacklightPrivate;

typedef struct
{
  GObject             parent;
  EspmBacklightPrivate       *priv;
} EspmBacklight;

typedef struct
{
  GObjectClass     parent_class;
} EspmBacklightClass;

GType              espm_backlight_get_type         (void) G_GNUC_CONST;
EspmBacklight     *espm_backlight_new              (void);
gboolean           espm_backlight_has_hw           (EspmBacklight *backlight);

G_END_DECLS

#endif /* __ESPM_BACKLIGHT_H */
