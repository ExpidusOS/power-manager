/*
 * * Copyright (C) 2008-2011 Ali <aliov@expidus.org>
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

#ifndef __ESPM_BUTTON_H
#define __ESPM_BUTTON_H

#include <glib-object.h>

#include "espm-enum-glib.h"

G_BEGIN_DECLS

#define ESPM_TYPE_BUTTON   (espm_button_get_type () )
#define ESPM_BUTTON(o)     (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_BUTTON, EspmButton))
#define ESPM_IS_BUTTON(o)  (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_BUTTON))

typedef struct EspmButtonPrivate EspmButtonPrivate;

typedef struct
{
  GObject             parent;
  EspmButtonPrivate  *priv;
} EspmButton;

typedef struct
{
    GObjectClass     parent_class;
    void            (*button_pressed)        (EspmButton *button,
                                              EspmButtonKey type);
} EspmButtonClass;

GType                     espm_button_get_type             (void) G_GNUC_CONST;
EspmButton               *espm_button_new                  (void);
guint16                   espm_button_get_mapped           (EspmButton *button) G_GNUC_PURE;

G_END_DECLS

#endif /* __ESPM_BUTTON_H */
