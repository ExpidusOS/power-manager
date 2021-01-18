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

#ifndef __ESPM_INHIBIT_H
#define __ESPM_INHIBIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_INHIBIT        (espm_inhibit_get_type () )
#define ESPM_INHIBIT(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_INHIBIT, EspmInhibit))
#define ESPM_IS_INHIBIT(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_INHIBIT))

typedef struct EspmInhibitPrivate EspmInhibitPrivate;

typedef struct
{
    GObject               parent;
    EspmInhibitPrivate   *priv;
} EspmInhibit;

typedef struct
{
    GObjectClass     parent_class;

    /* signals */
    void            (*has_inhibit_changed)       (EspmInhibit *inhibit,
                                                  gboolean is_inhibit);
    void            (*inhibitors_list_changed)   (EspmInhibit *inhibit,
                                                  gboolean is_inhibit);
} EspmInhibitClass;

GType              espm_inhibit_get_type         (void) G_GNUC_CONST;
GType              espm_inhibit_error_get_type   (void) G_GNUC_CONST;
GQuark             espm_inhibit_get_error_quark  ();
EspmInhibit       *espm_inhibit_new              (void);
const gchar      **espm_inhibit_get_inhibit_list (EspmInhibit *inhibit);

G_END_DECLS

#endif /* __ESPM_INHIBIT_H */
