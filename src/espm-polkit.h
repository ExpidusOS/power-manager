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

#ifndef __ESPM_POLKIT_H
#define __ESPM_POLKIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_POLKIT        (espm_polkit_get_type () )
#define ESPM_POLKIT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_POLKIT, EspmPolkit))
#define ESPM_IS_POLKIT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_POLKIT))

typedef struct EspmPolkitPrivate EspmPolkitPrivate;

typedef struct
{
  GObject              parent;
  EspmPolkitPrivate   *priv;
} EspmPolkit;

typedef struct
{
  GObjectClass      parent_class;
  void            (*auth_changed)    (EspmPolkit *polkit);

} EspmPolkitClass;

GType               espm_polkit_get_type          (void) G_GNUC_CONST;
EspmPolkit         *espm_polkit_get               (void);
gboolean            espm_polkit_check_auth        (EspmPolkit *polkit,
                                                   const gchar *action_id);

G_END_DECLS

#endif /* __ESPM_POLKIT_H */
