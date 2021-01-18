/* *
 *  Copyright (C) 2009-2011 Ali <aliov@expidus.org>
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

#ifndef __ESPM_DBUS_MONITOR_H
#define __ESPM_DBUS_MONITOR_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define ESPM_TYPE_DBUS_MONITOR        (espm_dbus_monitor_get_type () )
#define ESPM_DBUS_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_DBUS_MONITOR, EspmDBusMonitor))
#define ESPM_IS_DBUS_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_DBUS_MONITOR))

typedef struct EspmDBusMonitorPrivate EspmDBusMonitorPrivate;

typedef struct
{
  GObject                 parent;
  EspmDBusMonitorPrivate *priv;

} EspmDBusMonitor;

typedef struct
{
  GObjectClass 	parent_class;

  /*
   * Unique name connection lost.
   */
  void                (*unique_name_lost)              (EspmDBusMonitor *monitor,
                                                        gchar *unique_name,
                                                        gboolean on_session);
  /*
   * A Service connection changed.
   */
  void                (*service_connection_changed)    (EspmDBusMonitor *monitor,
                                                        gchar *service_name,
                                                        gboolean connected,
                                                        gboolean on_session);
  /*
   * DBus: system bus disconnected
   */
  void                (*system_bus_connection_changed) (EspmDBusMonitor *monitor,
                                                        gboolean connected);
} EspmDBusMonitorClass;

GType             espm_dbus_monitor_get_type           (void) G_GNUC_CONST;
EspmDBusMonitor  *espm_dbus_monitor_new                (void);
void              espm_dbus_monitor_watch_system_bus   (EspmDBusMonitor *monitor);
gboolean          espm_dbus_monitor_add_unique_name    (EspmDBusMonitor *monitor,
                                                        GBusType         bus_type,
                                                        const gchar     *unique_name);
void              espm_dbus_monitor_remove_unique_name (EspmDBusMonitor *monitor,
                                                        GBusType         bus_type,
                                                        const gchar     *unique_name);
gboolean          espm_dbus_monitor_add_service        (EspmDBusMonitor *monitor,
                                                        GBusType         bus_type,
                                                        const gchar     *service_name);
void              espm_dbus_monitor_remove_service     (EspmDBusMonitor *monitor,
                                                        GBusType         bus_type,
                                                        const gchar     *service_name);
G_END_DECLS

#endif /* __ESPM_DBUS_MONITOR_H */
