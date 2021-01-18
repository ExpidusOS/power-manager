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

#ifndef __ESPM_MANAGER_H
#define __ESPM_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_TYPE_MANAGER          (espm_manager_get_type () )
#define ESPM_MANAGER(o)            (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_MANAGER, EspmManager))
#define ESPM_IS_MANAGER(o)         (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_MANAGER))
#define ESPM_MANAGER_GET_CLASS(k)  (G_TYPE_INSTANCE_GET_CLASS((k), ESPM_TYPE_MANAGER, EspmManagerClass))

typedef struct EspmManagerPrivate EspmManagerPrivate;

typedef struct
{
  GObject      parent;
  EspmManagerPrivate   *priv;
} EspmManager;

typedef struct
{
  GObjectClass     parent_class;
} EspmManagerClass;

GType              espm_manager_get_type        (void) G_GNUC_CONST;
EspmManager       *espm_manager_new             (GDBusConnection *bus,
                                                 const gchar *client_id);
void               espm_manager_start           (EspmManager *manager);
void               espm_manager_stop            (EspmManager *manager);
GHashTable        *espm_manager_get_config      (EspmManager *manager);

G_END_DECLS

#endif /* __ESPM_MANAGER_H */
