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

#ifndef __ESPM_BRIGHTNESS_H
#define __ESPM_BRIGHTNESS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_BRIGHTNESS        (espm_brightness_get_type () )
#define ESPM_BRIGHTNESS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_BRIGHTNESS, EspmBrightness))
#define ESPM_IS_BRIGHTNESS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_BRIGHTNESS))

typedef struct EspmBrightnessPrivate EspmBrightnessPrivate;

typedef struct
{
  GObject                     parent;
  EspmBrightnessPrivate      *priv;

} EspmBrightness;

typedef struct
{
  GObjectClass 		parent_class;

} EspmBrightnessClass;

GType             espm_brightness_get_type        (void) G_GNUC_CONST;
EspmBrightness   *espm_brightness_new             (void);
gboolean          espm_brightness_setup           (EspmBrightness *brightness);
gboolean          espm_brightness_up              (EspmBrightness *brightness,
                                                   gint32         *new_level);
gboolean          espm_brightness_down            (EspmBrightness *brightness,
                                                   gint32         *new_level);
gboolean          espm_brightness_has_hw          (EspmBrightness *brightness);
gint32            espm_brightness_get_max_level   (EspmBrightness *brightness);
gboolean          espm_brightness_get_level       (EspmBrightness *brightness,
                                                   gint32         *level);
gboolean          espm_brightness_set_level       (EspmBrightness *brightness,
                                                   gint32          level);
gboolean          espm_brightness_set_step_count  (EspmBrightness *brightness,
                                                   guint32         count,
                                                   gboolean        exponential);
gboolean          espm_brightness_dim_down        (EspmBrightness *brightness);
gboolean          espm_brightness_get_switch      (EspmBrightness *brightness,
                                                   gint           *brightness_switch);
gboolean          espm_brightness_set_switch      (EspmBrightness *brightness,
                                                   gint            brightness_switch);

G_END_DECLS

#endif /* __ESPM_BRIGHTNESS_H */
