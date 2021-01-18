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

#ifndef __ESPM_DPMS_H
#define __ESPM_DPMS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include <esconf/esconf.h>

#include <gdk/gdkx.h>
#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>

G_BEGIN_DECLS

#define ESPM_TYPE_DPMS        (espm_dpms_get_type () )
#define ESPM_DPMS(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_DPMS, EspmDpms))
#define ESPM_IS_DPMS(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_DPMS))

typedef struct EspmDpmsPrivate EspmDpmsPrivate;

typedef struct
{
  GObject            parent;
  EspmDpmsPrivate   *priv;
} EspmDpms;

typedef struct
{
  GObjectClass       parent_class;
} EspmDpmsClass;

GType           espm_dpms_get_type        (void) G_GNUC_CONST;
EspmDpms       *espm_dpms_new             (void);
gboolean        espm_dpms_capable         (EspmDpms *dpms) G_GNUC_PURE;
void            espm_dpms_force_level     (EspmDpms *dpms, CARD16 level);
void            espm_dpms_refresh         (EspmDpms *dpms);
void            espm_dpms_inhibit         (EspmDpms *dpms, gboolean inhibit);
gboolean        espm_dpms_is_inhibited    (EspmDpms *dpms);
void            espm_dpms_set_on_battery  (EspmDpms *dpms, gboolean on_battery);

G_END_DECLS


#endif /* __ESPM_DPMS_H */
