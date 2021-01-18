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

#ifndef __ESPM_BATTERY_H
#define __ESPM_BATTERY_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <upower.h>

#include "espm-enum-glib.h"

G_BEGIN_DECLS

#define ESPM_TYPE_BATTERY        (espm_battery_get_type () )
#define ESPM_BATTERY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_BATTERY, EspmBattery))
#define ESPM_IS_BATTERY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_BATTERY))

typedef struct EspmBatteryPrivate EspmBatteryPrivate;

typedef struct
{
    GtkWidget                parent;
    EspmBatteryPrivate     *priv;
} EspmBattery;

typedef struct
{
    GtkWidgetClass       parent_class;
    void              (*battery_charge_changed)   (EspmBattery *battery);
} EspmBatteryClass;

GType               espm_battery_get_type         (void) G_GNUC_CONST;
GtkWidget          *espm_battery_new              (void);
void                espm_battery_monitor_device   (EspmBattery *battery,
                                                   const char *object_path,
                                                   UpDeviceKind device_type);
UpDeviceKind        espm_battery_get_device_type  (EspmBattery *battery);
EspmBatteryCharge   espm_battery_get_charge       (EspmBattery *battery);
const gchar        *espm_battery_get_battery_name (EspmBattery *battery);
gchar              *espm_battery_get_time_left    (EspmBattery *battery);
const gchar        *espm_battery_get_icon_name    (EspmBattery *battery);

G_END_DECLS

#endif /* __ESPM_BATTERY_H */
