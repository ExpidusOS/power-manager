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

#ifndef __ESPM_ESCONF_H
#define __ESPM_ESCONF_H

#include <glib-object.h>
#include <esconf/esconf.h>

G_BEGIN_DECLS

#define ESPM_TYPE_ESCONF        (espm_esconf_get_type () )
#define ESPM_ESCONF(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_ESCONF, EspmEsconf))
#define ESPM_IS_ESCONF(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_ESCONF))

typedef struct  EspmEsconfPrivate EspmEsconfPrivate;

typedef struct
{
  GObject               parent;
  EspmEsconfPrivate    *priv;
} EspmEsconf;

typedef struct
{
  GObjectClass          parent_class;
} EspmEsconfClass;

GType              espm_esconf_get_type             (void) G_GNUC_CONST;
EspmEsconf        *espm_esconf_new                  (void);
EsconfChannel     *espm_esconf_get_channel          (EspmEsconf *conf);

G_END_DECLS

#endif /* __ESPM_ESCONF_H */
