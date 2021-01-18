/* -*- c-basic-offset: 4 -*- vi:set ts=4 sts=4 sw=4:
 * * Copyright (C) 2015 Expidus Development Team <expidus1-dev@expidus.org>
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

#include <gtk/gtk.h>

#include "expidus-power-manager-dbus.h"


#ifndef __ESPM_SETTINGS_APP_H
#define __ESPM_SETTINGS_APP_H

G_BEGIN_DECLS


#define ESPM_TYPE_SETTINGS_APP            (espm_settings_app_get_type())
#define ESPM_SETTINGS_APP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), ESPM_TYPE_SETTINGS_APP, EspmSettingsApp))
#define ESPM_SETTINGS_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ESPM_TYPE_SETTINGS_APP, EspmSettingsAppClass))
#define ESPM_IS_SETTINGS_APP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ESPM_TYPE_SETTINGS_APP))
#define ESPM_SETTINGS_APP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), ESPM_TYPE_SETTINGS_APP, EspmSettingsAppClass))

typedef struct _EspmSettingsApp         EspmSettingsApp;
typedef struct _EspmSettingsAppClass    EspmSettingsAppClass;
typedef struct _EspmSettingsAppPrivate  EspmSettingsAppPrivate;

struct _EspmSettingsApp
{
  GtkApplication               parent;
  EspmSettingsAppPrivate      *priv;
};

struct _EspmSettingsAppClass
{
  GtkApplicationClass    parent_class;
};


GType                espm_settings_app_get_type        (void) G_GNUC_CONST;
EspmSettingsApp     *espm_settings_app_new             (void);


G_END_DECLS

#endif /* __ESPM_SETTINGS_APP_H */
