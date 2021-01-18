/*
 * * Copyright (C) 2009-2011 Ali <aliov@expidus.org>
 * * Copyright (C) 2013 Andreas Müller <schnitzeltony@googlemail.com>
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

#ifndef __ESPM_SYSTEMD_H
#define __ESPM_SYSTEMD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

#define ESPM_TYPE_SYSTEMD            (espm_systemd_get_type () )
#define ESPM_SYSTEMD(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_SYSTEMD, EspmSystemd))
#define ESPM_IS_SYSTEMD(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_SYSTEMD))

typedef struct EspmSystemdPrivate EspmSystemdPrivate;

typedef struct
{
    GObject                 parent;
    EspmSystemdPrivate      *priv;

} EspmSystemd;

typedef struct
{
    GObjectClass        parent_class;

} EspmSystemdClass;

GType               espm_systemd_get_type   (void) G_GNUC_CONST;

EspmSystemd         *espm_systemd_new   (void);

void                espm_systemd_shutdown   (EspmSystemd *systemd,
                                             GError **error);

void                espm_systemd_reboot (EspmSystemd *systemd,
                                         GError **error);

void                espm_systemd_sleep (EspmSystemd *systemd,
                                        const gchar *method,
                                        GError **error);

G_END_DECLS

#endif /* __ESPM_SYSTEMD_H */
