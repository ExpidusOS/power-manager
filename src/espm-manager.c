/*
 * * Copyright (C) 2008-2011 Ali <aliov@expidus.org>
 * * Copyright (C) 2019 Kacper Piwiński
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>
#include <esconf/esconf.h>

#include <gio/gunixfdlist.h>

#include <libnotify/notify.h>

#include "espm-power.h"
#include "espm-dbus.h"
#include "espm-dpms.h"
#include "espm-manager.h"
#include "espm-console-kit.h"
#include "espm-button.h"
#include "espm-backlight.h"
#include "espm-kbd-backlight.h"
#include "espm-inhibit.h"
#include "egg-idletime.h"
#include "espm-config.h"
#include "espm-debug.h"
#include "espm-esconf.h"
#include "espm-errors.h"
#include "espm-common.h"
#include "espm-enum.h"
#include "espm-enum-glib.h"
#include "espm-enum-types.h"
#include "espm-dbus-monitor.h"
#include "espm-systemd.h"
#include "expidus-screensaver.h"
#include "../panel-plugins/power-manager-plugin/power-manager-button.h"

static void espm_manager_finalize   (GObject *object);
static void espm_manager_set_property(GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec);
static void espm_manager_get_property(GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec);

static void espm_manager_dbus_class_init (EspmManagerClass *klass);
static void espm_manager_dbus_init   (EspmManager *manager);

static gboolean espm_manager_quit (EspmManager *manager);

static void espm_manager_show_tray_icon (EspmManager *manager);
static void espm_manager_hide_tray_icon (EspmManager *manager);

#define SLEEP_KEY_TIMEOUT 6.0f

struct EspmManagerPrivate
{
  GDBusConnection    *session_bus;
  GDBusConnection    *system_bus;

  ExpidusSMClient       *client;

  EspmPower          *power;
  EspmButton         *button;
  EspmEsconf         *conf;
  EspmBacklight      *backlight;
  EspmKbdBacklight   *kbd_backlight;
  EspmConsoleKit     *console;
  EspmSystemd        *systemd;
  EspmDBusMonitor    *monitor;
  EspmInhibit        *inhibit;
  ExpidusScreenSaver    *screensaver;
  EggIdletime        *idle;
  GtkStatusIcon      *adapter_icon;
  GtkWidget          *power_button;
  gint                show_tray_icon;

  EspmDpms           *dpms;

  GTimer         *timer;

  gboolean          inhibited;
  gboolean          session_managed;

  gint                inhibit_fd;
};

enum
{
  PROP_0 = 0,
  PROP_SHOW_TRAY_ICON
};

G_DEFINE_TYPE_WITH_PRIVATE (EspmManager, espm_manager, G_TYPE_OBJECT)

static void
espm_manager_class_init (EspmManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = espm_manager_finalize;
  object_class->set_property = espm_manager_set_property;
  object_class->get_property = espm_manager_get_property;

#define ESPM_PARAM_FLAGS  (G_PARAM_READWRITE \
                     | G_PARAM_CONSTRUCT \
                     | G_PARAM_STATIC_NAME \
                     | G_PARAM_STATIC_NICK \
                     | G_PARAM_STATIC_BLURB)

  g_object_class_install_property (object_class, PROP_SHOW_TRAY_ICON,
                             g_param_spec_int (SHOW_TRAY_ICON_CFG,
                                               SHOW_TRAY_ICON_CFG,
                                               SHOW_TRAY_ICON_CFG,
                                               0, 5, 0,
                                               ESPM_PARAM_FLAGS));
#undef ESPM_PARAM_FLAGS
}

static void
espm_manager_init (EspmManager *manager)
{
  manager->priv = espm_manager_get_instance_private (manager);

  manager->priv->timer = g_timer_new ();

  notify_init ("expidus1-power-manager");
}

static void
espm_manager_finalize (GObject *object)
{
  EspmManager *manager;

  manager = ESPM_MANAGER(object);

  if ( manager->priv->session_bus )
    g_object_unref (manager->priv->session_bus);

  if ( manager->priv->system_bus )
    g_object_unref (manager->priv->system_bus);

  g_object_unref (manager->priv->power);
  g_object_unref (manager->priv->button);
  g_object_unref (manager->priv->conf);
  g_object_unref (manager->priv->client);
  if ( manager->priv->systemd != NULL )
    g_object_unref (manager->priv->systemd);
  if ( manager->priv->console != NULL )
    g_object_unref (manager->priv->console);
  g_object_unref (manager->priv->monitor);
  g_object_unref (manager->priv->inhibit);
  g_object_unref (manager->priv->idle);

  g_timer_destroy (manager->priv->timer);

  g_object_unref (manager->priv->dpms);

  g_object_unref (manager->priv->backlight);

  g_object_unref (manager->priv->kbd_backlight);

  G_OBJECT_CLASS (espm_manager_parent_class)->finalize (object);
}

static void
espm_manager_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EspmManager *manager = ESPM_MANAGER(object);
  gint new_value;

  switch(property_id) {
    case PROP_SHOW_TRAY_ICON:
      new_value = g_value_get_int (value);
      if (new_value != manager->priv->show_tray_icon)
      {
        manager->priv->show_tray_icon = new_value;
        if (new_value > 0)
        {
          espm_manager_show_tray_icon (manager);
        }
        else
        {
          espm_manager_hide_tray_icon (manager);
        }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
}

static void
espm_manager_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EspmManager *manager = ESPM_MANAGER(object);

  switch(property_id) {
    case PROP_SHOW_TRAY_ICON:
      g_value_set_int (value, manager->priv->show_tray_icon);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
}

static void
espm_manager_release_names (EspmManager *manager)
{
  espm_dbus_release_name (manager->priv->session_bus,
                          "com.expidus.PowerManager");

  espm_dbus_release_name (manager->priv->session_bus,
                          "org.freedesktop.PowerManagement");
}

static gboolean
espm_manager_quit (EspmManager *manager)
{
  ESPM_DEBUG ("Exiting");

  espm_manager_release_names (manager);

  if (manager->priv->inhibit_fd >= 0)
    close (manager->priv->inhibit_fd);

  gtk_main_quit ();
  return TRUE;
}

static void
espm_manager_system_bus_connection_changed_cb (EspmDBusMonitor *monitor, gboolean connected, EspmManager *manager)
{
  if ( connected == TRUE )
  {
    ESPM_DEBUG ("System bus connection changed to TRUE, restarting the power manager");
    espm_manager_quit (manager);
    g_spawn_command_line_async ("expidus1-power-manager", NULL);
  }
}

static gboolean
espm_manager_reserve_names (EspmManager *manager)
{
  if ( !espm_dbus_register_name (manager->priv->session_bus,
                                 "com.expidus.PowerManager") ||
       !espm_dbus_register_name (manager->priv->session_bus,
                                 "org.freedesktop.PowerManagement") )
  {
    g_warning ("Unable to reserve bus name: Maybe any already running instance?\n");

    g_object_unref (G_OBJECT (manager));
    gtk_main_quit ();

    return FALSE;
  }
  return TRUE;
}

static void
espm_manager_shutdown (EspmManager *manager)
{
  GError *error = NULL;

  if ( LOGIND_RUNNING () )
    espm_systemd_shutdown (manager->priv->systemd, &error );
  else
    espm_console_kit_shutdown (manager->priv->console, &error );

  if ( error )
  {
    g_warning ("Failed to shutdown the system : %s", error->message);
    g_error_free (error);
    /* Try with the session then */
    if ( manager->priv->session_managed )
      expidus_sm_client_request_shutdown (manager->priv->client, EXPIDUS_SM_CLIENT_SHUTDOWN_HINT_HALT);
  }
}

static void
espm_manager_ask_shutdown (EspmManager *manager)
{
  if ( manager->priv->session_managed )
  expidus_sm_client_request_shutdown (manager->priv->client, EXPIDUS_SM_CLIENT_SHUTDOWN_HINT_ASK);
}

static void
espm_manager_sleep_request (EspmManager *manager, EspmShutdownRequest req, gboolean force)
{
  switch (req)
  {
    case ESPM_DO_NOTHING:
      break;
    case ESPM_DO_SUSPEND:
      espm_power_suspend (manager->priv->power, force);
      break;
    case ESPM_DO_HIBERNATE:
      espm_power_hibernate (manager->priv->power, force);
      break;
    case ESPM_DO_SHUTDOWN:
      espm_manager_shutdown (manager);
      break;
    case ESPM_ASK:
      espm_manager_ask_shutdown (manager);
      break;
    default:
      g_warn_if_reached ();
      break;
  }
}

static void
espm_manager_reset_sleep_timer (EspmManager *manager)
{
  g_timer_reset (manager->priv->timer);
}

static void
espm_manager_button_pressed_cb (EspmButton *bt, EspmButtonKey type, EspmManager *manager)
{
  EspmShutdownRequest req = ESPM_DO_NOTHING;

  ESPM_DEBUG_ENUM (type, ESPM_TYPE_BUTTON_KEY, "Received button press event");

  if ( type == BUTTON_MON_BRIGHTNESS_DOWN || type == BUTTON_MON_BRIGHTNESS_UP )
    return;

  if ( type == BUTTON_KBD_BRIGHTNESS_DOWN || type == BUTTON_KBD_BRIGHTNESS_UP )
    return;

  if ( type == BUTTON_POWER_OFF )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  POWER_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_SLEEP )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  SLEEP_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_HIBERNATE )
    {
    g_object_get (G_OBJECT (manager->priv->conf),
                  HIBERNATE_SWITCH_CFG, &req,
                  NULL);
  }
  else if ( type == BUTTON_BATTERY )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  BATTERY_SWITCH_CFG, &req,
                  NULL);
  }
  else
  {
    g_return_if_reached ();
  }

  ESPM_DEBUG_ENUM (req, ESPM_TYPE_SHUTDOWN_REQUEST, "Shutdown request : ");

  if ( req == ESPM_ASK )
    espm_manager_ask_shutdown (manager);
  else
  {
    if ( g_timer_elapsed (manager->priv->timer, NULL) > SLEEP_KEY_TIMEOUT )
    {
      g_timer_reset (manager->priv->timer);
      espm_manager_sleep_request (manager, req, FALSE);
    }
  }
}

static void
espm_manager_lid_changed_cb (EspmPower *power, gboolean lid_is_closed, EspmManager *manager)
{
  EspmLidTriggerAction action;
  gboolean on_battery, logind_handle_lid_switch;

  if ( LOGIND_RUNNING() )
  {
    g_object_get (G_OBJECT (manager->priv->conf),
                  LOGIND_HANDLE_LID_SWITCH, &logind_handle_lid_switch,
                  NULL);

    if (logind_handle_lid_switch)
      return;
  }

  g_object_get (G_OBJECT (power),
                "on-battery", &on_battery,
                NULL);

  g_object_get (G_OBJECT (manager->priv->conf),
                on_battery ? LID_SWITCH_ON_BATTERY_CFG : LID_SWITCH_ON_AC_CFG, &action,
                NULL);

  if ( lid_is_closed )
  {
    ESPM_DEBUG_ENUM (action, ESPM_TYPE_LID_TRIGGER_ACTION, "LID close event");

    if ( action == LID_TRIGGER_NOTHING )
    {
      if ( !espm_is_multihead_connected () )
        espm_dpms_force_level (manager->priv->dpms, DPMSModeOff);
    }
    else if ( action == LID_TRIGGER_LOCK_SCREEN )
    {
      if ( !espm_is_multihead_connected () )
      {
        if (!expidus_screensaver_lock (manager->priv->screensaver))
        {
          expidus_dialog_show_error (NULL, NULL,
                                  _("None of the screen lock tools ran "
                                    "successfully, the screen will not "
                                    "be locked."));
        }
      }
    }
    else
    {
      /*
       * Force sleep here as lid is closed and no point of asking the
       * user for confirmation in case of an application is inhibiting
       * the power manager.
       */
      espm_manager_sleep_request (manager, action, TRUE);
    }
  }
  else
  {
    ESPM_DEBUG_ENUM (action, ESPM_TYPE_LID_TRIGGER_ACTION, "LID opened");

    espm_dpms_force_level (manager->priv->dpms, DPMSModeOn);
  }
}

static void
espm_manager_inhibit_changed_cb (EspmInhibit *inhibit, gboolean inhibited, EspmManager *manager)
{
  manager->priv->inhibited = inhibited;
}

static void
espm_manager_alarm_timeout_cb (EggIdletime *idle, guint id, EspmManager *manager)
{
  if (espm_power_is_in_presentation_mode (manager->priv->power) == TRUE)
    return;

  ESPM_DEBUG ("Alarm inactivity timeout id %d", id);

  if ( id == TIMEOUT_INACTIVITY_ON_AC || id == TIMEOUT_INACTIVITY_ON_BATTERY )
  {
    EspmShutdownRequest sleep_mode = ESPM_DO_NOTHING;
    gboolean on_battery;

    if ( manager->priv->inhibited )
    {
      ESPM_DEBUG ("Idle sleep alarm timeout, but power manager is currently inhibited, action ignored");
      return;
    }

    if ( id == TIMEOUT_INACTIVITY_ON_AC)
      g_object_get (G_OBJECT (manager->priv->conf),
                    INACTIVITY_SLEEP_MODE_ON_AC, &sleep_mode,
                    NULL);
    else
      g_object_get (G_OBJECT (manager->priv->conf),
                    INACTIVITY_SLEEP_MODE_ON_BATTERY, &sleep_mode,
                    NULL);

    g_object_get (G_OBJECT (manager->priv->power),
                  "on-battery", &on_battery,
                  NULL);

    if ( id == TIMEOUT_INACTIVITY_ON_AC && on_battery == FALSE )
      espm_manager_sleep_request (manager, sleep_mode, FALSE);
    else if ( id ==  TIMEOUT_INACTIVITY_ON_BATTERY && on_battery )
      espm_manager_sleep_request (manager, sleep_mode, FALSE);
  }
}

static void
espm_manager_set_idle_alarm_on_ac (EspmManager *manager)
{
  guint on_ac;

  g_object_get (G_OBJECT (manager->priv->conf),
                ON_AC_INACTIVITY_TIMEOUT, &on_ac,
                NULL);

#ifdef DEBUG
  if ( on_ac == 14 )
    TRACE ("setting inactivity sleep timeout on ac to never");
  else
    TRACE ("setting inactivity sleep timeout on ac to %d", on_ac);
#endif

  if ( on_ac == 14 )
  {
    egg_idletime_alarm_remove (manager->priv->idle, TIMEOUT_INACTIVITY_ON_AC );
  }
  else
  {
    egg_idletime_alarm_set (manager->priv->idle, TIMEOUT_INACTIVITY_ON_AC, on_ac * 1000 * 60);
  }
}

static void
espm_manager_set_idle_alarm_on_battery (EspmManager *manager)
{
  guint on_battery;

  g_object_get (G_OBJECT (manager->priv->conf),
                ON_BATTERY_INACTIVITY_TIMEOUT, &on_battery,
                NULL);

#ifdef DEBUG
  if ( on_battery == 14 )
    TRACE ("setting inactivity sleep timeout on battery to never");
  else
    TRACE ("setting inactivity sleep timeout on battery to %d", on_battery);
#endif

  if ( on_battery == 14 )
  {
    egg_idletime_alarm_remove (manager->priv->idle, TIMEOUT_INACTIVITY_ON_BATTERY );
  }
  else
  {
    egg_idletime_alarm_set (manager->priv->idle, TIMEOUT_INACTIVITY_ON_BATTERY, on_battery * 1000 * 60);
  }
}

static void
espm_manager_on_battery_changed_cb (EspmPower *power, gboolean on_battery, EspmManager *manager)
{
  egg_idletime_alarm_reset_all (manager->priv->idle);
}

static void
espm_manager_set_idle_alarm (EspmManager *manager)
{
  espm_manager_set_idle_alarm_on_ac (manager);
  espm_manager_set_idle_alarm_on_battery (manager);
}

static gchar*
espm_manager_get_systemd_events(EspmManager *manager)
{
  GSList *events = NULL;
  gchar *what = "";
  gboolean logind_handle_power_key, logind_handle_suspend_key, logind_handle_hibernate_key, logind_handle_lid_switch;

  g_object_get (G_OBJECT (manager->priv->conf),
                LOGIND_HANDLE_POWER_KEY, &logind_handle_power_key,
                LOGIND_HANDLE_SUSPEND_KEY, &logind_handle_suspend_key,
                LOGIND_HANDLE_HIBERNATE_KEY, &logind_handle_hibernate_key,
                LOGIND_HANDLE_LID_SWITCH, &logind_handle_lid_switch,
                NULL);

  if (!logind_handle_power_key)
    events = g_slist_append(events, "handle-power-key");
  if (!logind_handle_suspend_key)
    events = g_slist_append(events, "handle-suspend-key");
  if (!logind_handle_hibernate_key)
    events = g_slist_append(events, "handle-hibernate-key");
  if (!logind_handle_lid_switch)
    events = g_slist_append(events, "handle-lid-switch");

  while (events != NULL)
  {
    if ( g_strcmp0 (what, "") == 0 )
      what = g_strdup ( (gchar *) events->data );
    else
      what = g_strconcat (what, ":", (gchar *) events->data, NULL);
    events = g_slist_next (events);
  }
  g_slist_free(events);

  return what;
}

static gint
espm_manager_inhibit_sleep_systemd (EspmManager *manager)
{
  GDBusConnection *bus_connection;
  GVariant *reply;
  GError *error = NULL;
  GUnixFDList *fd_list = NULL;
  gint fd = -1;
  char *what = espm_manager_get_systemd_events(manager);
  const char *who = "expidus1-power-manager";
  const char *why = "expidus1-power-manager handles these events";
  const char *mode = "block";

  if (g_strcmp0(what, "") == 0)
    return -1;

  if (!(LOGIND_RUNNING()))
    return -1;

  ESPM_DEBUG ("Inhibiting systemd sleep: %s", what);

  bus_connection = manager->priv->system_bus;
  if (!espm_dbus_name_has_owner (bus_connection, "org.freedesktop.login1"))
    return -1;

  reply = g_dbus_connection_call_with_unix_fd_list_sync (bus_connection,
                                                         "org.freedesktop.login1",
                                                         "/org/freedesktop/login1",
                                                         "org.freedesktop.login1.Manager",
                                                         "Inhibit",
                                                         g_variant_new ("(ssss)",
                                                                        what, who, why, mode),
                                                         G_VARIANT_TYPE ("(h)"),
                                                         G_DBUS_CALL_FLAGS_NONE,
                                                         -1,
                                                         NULL,
                                                         &fd_list,
                                                         NULL,
                                                         &error);

  if (!reply)
  {
    if (error)
    {
      g_warning ("Unable to inhibit systemd sleep: %s", error->message);
      g_error_free (error);
    }
    return -1;
  }

  g_variant_unref (reply);

  fd = g_unix_fd_list_get (fd_list, 0, &error);
  if (fd == -1)
  {
    g_warning ("Inhibit() reply parsing failed: %s", error->message);
  }

  g_object_unref (fd_list);

  if (error)
    g_error_free (error);

  g_free (what);

  return fd;
}

static void
espm_manager_systemd_events_changed (EspmManager *manager)
{
  if (manager->priv->inhibit_fd >= 0)
    close (manager->priv->inhibit_fd);

  if (manager->priv->system_bus)
    manager->priv->inhibit_fd = espm_manager_inhibit_sleep_systemd (manager);
}

static void
espm_manager_tray_update_tooltip (PowerManagerButton *button, EspmManager *manager)
{
  g_return_if_fail (ESPM_IS_MANAGER (manager));
  g_return_if_fail (POWER_MANAGER_IS_BUTTON (manager->priv->power_button));
  g_return_if_fail (GTK_IS_STATUS_ICON (manager->priv->adapter_icon));

  ESPM_DEBUG ("updating tooltip");

  if (power_manager_button_get_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button)) == NULL)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_tooltip_markup (manager->priv->adapter_icon, power_manager_button_get_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button)));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
espm_manager_tray_update_icon (PowerManagerButton *button, EspmManager *manager)
{
  g_return_if_fail (ESPM_IS_MANAGER (manager));
  g_return_if_fail (POWER_MANAGER_IS_BUTTON (manager->priv->power_button));

  ESPM_DEBUG ("updating icon");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_from_icon_name (manager->priv->adapter_icon, power_manager_button_get_icon_name (POWER_MANAGER_BUTTON(manager->priv->power_button)));
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
espm_manager_show_tray_menu (GtkStatusIcon *icon, guint button, guint activate_time, EspmManager *manager)
{
  power_manager_button_show_menu (POWER_MANAGER_BUTTON(manager->priv->power_button));
}

static void
espm_manager_show_tray_icon (EspmManager *manager)
{
  if (manager->priv->adapter_icon != NULL)
  {
    ESPM_DEBUG ("tray icon already being shown");
    return;
  }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  manager->priv->adapter_icon = gtk_status_icon_new ();
  manager->priv->power_button = power_manager_button_new ();
G_GNUC_END_IGNORE_DEPRECATIONS

  ESPM_DEBUG ("Showing tray icon");

  /* send a show event to startup the button */
  power_manager_button_show (POWER_MANAGER_BUTTON(manager->priv->power_button));

  /* initial update the tray icon + tooltip */
  espm_manager_tray_update_icon (POWER_MANAGER_BUTTON(manager->priv->power_button), manager);
  espm_manager_tray_update_tooltip (POWER_MANAGER_BUTTON(manager->priv->power_button), manager);

  /* Listen to the tooltip and icon changes */
  g_signal_connect (G_OBJECT(manager->priv->power_button), "tooltip-changed",   G_CALLBACK(espm_manager_tray_update_tooltip), manager);
  g_signal_connect (G_OBJECT(manager->priv->power_button), "icon-name-changed", G_CALLBACK(espm_manager_tray_update_icon),    manager);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_visible (manager->priv->adapter_icon, TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_signal_connect (manager->priv->adapter_icon, "popup-menu", G_CALLBACK (espm_manager_show_tray_menu), manager);
}

static void
espm_manager_hide_tray_icon (EspmManager *manager)
{
  if (manager->priv->adapter_icon == NULL)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_visible (manager->priv->adapter_icon, FALSE);
G_GNUC_END_IGNORE_DEPRECATIONS

    /* disconnect from all the signals */
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->power_button), G_CALLBACK(espm_manager_tray_update_tooltip), manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->power_button), G_CALLBACK(espm_manager_tray_update_icon),    manager);
  g_signal_handlers_disconnect_by_func (G_OBJECT(manager->priv->adapter_icon), G_CALLBACK(espm_manager_show_tray_menu),      manager);

  g_object_unref (manager->priv->power_button);
  g_object_unref (manager->priv->adapter_icon);

  manager->priv->power_button = NULL;
  manager->priv->adapter_icon = NULL;
}

EspmManager *
espm_manager_new (GDBusConnection *bus, const gchar *client_id)
{
  EspmManager *manager = NULL;
  GError *error = NULL;
  gchar *current_dir;

  const gchar *restart_command[] =
  {
    "expidus1-power-manager",
    "--restart",
    NULL
  };

  manager = g_object_new (ESPM_TYPE_MANAGER, NULL);

  manager->priv->session_bus = bus;

  current_dir = g_get_current_dir ();
  manager->priv->client = expidus_sm_client_get_full (EXPIDUS_SM_CLIENT_RESTART_NORMAL,
                                                   EXPIDUS_SM_CLIENT_PRIORITY_DEFAULT,
                                                   client_id,
                                                   current_dir,
                                                   restart_command,
                                                   SYSCONFDIR "/xdg/autostart/" PACKAGE_NAME ".desktop");

  g_free (current_dir);

  manager->priv->session_managed = expidus_sm_client_connect (manager->priv->client, &error);

  if ( error )
  {
    g_warning ("Unable to connect to session manager : %s", error->message);
    g_error_free (error);
  }
  else
  {
    g_signal_connect_swapped (manager->priv->client, "quit",
                              G_CALLBACK (espm_manager_quit), manager);
  }

  espm_manager_dbus_class_init (ESPM_MANAGER_GET_CLASS (manager));
  espm_manager_dbus_init (manager);

  return manager;
}

void espm_manager_start (EspmManager *manager)
{
  GError *error = NULL;

  if ( !espm_manager_reserve_names (manager) )
  goto out;

  manager->priv->power = espm_power_get ();
  manager->priv->button = espm_button_new ();
  manager->priv->conf = espm_esconf_new ();
  manager->priv->screensaver = expidus_screensaver_new ();
  manager->priv->console = NULL;
  manager->priv->systemd = NULL;

  if ( LOGIND_RUNNING () )
    manager->priv->systemd = espm_systemd_new ();
  else
    manager->priv->console = espm_console_kit_new ();

  manager->priv->monitor = espm_dbus_monitor_new ();
  manager->priv->inhibit = espm_inhibit_new ();
  manager->priv->idle = egg_idletime_new ();

    /* Don't allow systemd to handle power/suspend/hibernate buttons
     * and lid-switch */
  manager->priv->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (manager->priv->system_bus)
    manager->priv->inhibit_fd = espm_manager_inhibit_sleep_systemd (manager);
  else
  {
    g_warning ("Unable connect to system bus: %s", error->message);
    g_clear_error (&error);
  }

  g_signal_connect (manager->priv->idle, "alarm-expired",
                    G_CALLBACK (espm_manager_alarm_timeout_cb), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" ON_AC_INACTIVITY_TIMEOUT,
                            G_CALLBACK (espm_manager_set_idle_alarm_on_ac), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" ON_BATTERY_INACTIVITY_TIMEOUT,
                            G_CALLBACK (espm_manager_set_idle_alarm_on_battery), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_POWER_KEY,
                            G_CALLBACK (espm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_SUSPEND_KEY,
                            G_CALLBACK (espm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_HIBERNATE_KEY,
                            G_CALLBACK (espm_manager_systemd_events_changed), manager);
  g_signal_connect_swapped (manager->priv->conf, "notify::" LOGIND_HANDLE_LID_SWITCH,
                            G_CALLBACK (espm_manager_systemd_events_changed), manager);

  espm_manager_set_idle_alarm (manager);

  g_signal_connect (manager->priv->inhibit, "has-inhibit-changed",
                    G_CALLBACK (espm_manager_inhibit_changed_cb), manager);
  g_signal_connect (manager->priv->monitor, "system-bus-connection-changed",
                    G_CALLBACK (espm_manager_system_bus_connection_changed_cb), manager);

  manager->priv->backlight = espm_backlight_new ();

  manager->priv->kbd_backlight = espm_kbd_backlight_new ();

  manager->priv->dpms = espm_dpms_new ();

  g_signal_connect (manager->priv->button, "button_pressed",
                    G_CALLBACK (espm_manager_button_pressed_cb), manager);

  g_signal_connect (manager->priv->power, "lid-changed",
                    G_CALLBACK (espm_manager_lid_changed_cb), manager);

  g_signal_connect (manager->priv->power, "on-battery-changed",
                    G_CALLBACK (espm_manager_on_battery_changed_cb), manager);

  g_signal_connect_swapped (manager->priv->power, "waking-up",
                            G_CALLBACK (espm_manager_reset_sleep_timer), manager);

  g_signal_connect_swapped (manager->priv->power, "sleeping",
                            G_CALLBACK (espm_manager_reset_sleep_timer), manager);

  g_signal_connect_swapped (manager->priv->power, "ask-shutdown",
                            G_CALLBACK (espm_manager_ask_shutdown), manager);

  g_signal_connect_swapped (manager->priv->power, "shutdown",
                            G_CALLBACK (espm_manager_shutdown), manager);

  esconf_g_property_bind (espm_esconf_get_channel (manager->priv->conf),
                                                   ESPM_PROPERTIES_PREFIX SHOW_TRAY_ICON_CFG,
                                                   G_TYPE_INT,
                                                   G_OBJECT(manager),
                                                   SHOW_TRAY_ICON_CFG);
out:
  ;
}

void espm_manager_stop (EspmManager *manager)
{
  ESPM_DEBUG ("Stopping");
  g_return_if_fail (ESPM_IS_MANAGER (manager));
  espm_manager_quit (manager);
}

GHashTable *espm_manager_get_config (EspmManager *manager)
{
  GHashTable *hash;

  guint16 mapped_buttons;
  gboolean auth_hibernate = FALSE;
  gboolean auth_suspend = FALSE;
  gboolean can_suspend = FALSE;
  gboolean can_hibernate = FALSE;
  gboolean has_sleep_button = FALSE;
  gboolean has_hibernate_button = FALSE;
  gboolean has_power_button = FALSE;
  gboolean has_battery_button = FALSE;
  gboolean has_battery = TRUE;
  gboolean has_lcd_brightness = TRUE;
  gboolean can_shutdown = TRUE;
  gboolean has_lid = FALSE;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (manager->priv->systemd),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (manager->priv->console),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }

  g_object_get (G_OBJECT (manager->priv->power),
                "auth-suspend", &auth_suspend,
                "auth-hibernate", &auth_hibernate,
                "can-suspend", &can_suspend,
                "can-hibernate", &can_hibernate,
                "has-lid", &has_lid,
                NULL);

  has_battery = espm_power_has_battery (manager->priv->power);
  has_lcd_brightness = espm_backlight_has_hw (manager->priv->backlight);

  mapped_buttons = espm_button_get_mapped (manager->priv->button);

  if ( mapped_buttons & SLEEP_KEY )
    has_sleep_button = TRUE;
  if ( mapped_buttons & HIBERNATE_KEY )
    has_hibernate_button = TRUE;
  if ( mapped_buttons & POWER_KEY )
    has_power_button = TRUE;
  if ( mapped_buttons & BATTERY_KEY )
    has_battery_button = TRUE;

  g_hash_table_insert (hash, g_strdup ("sleep-button"), g_strdup (espm_bool_to_string (has_sleep_button)));
  g_hash_table_insert (hash, g_strdup ("power-button"), g_strdup (espm_bool_to_string (has_power_button)));
  g_hash_table_insert (hash, g_strdup ("hibernate-button"), g_strdup (espm_bool_to_string (has_hibernate_button)));
  g_hash_table_insert (hash, g_strdup ("battery-button"), g_strdup (espm_bool_to_string (has_battery_button)));
  g_hash_table_insert (hash, g_strdup ("auth-suspend"), g_strdup (espm_bool_to_string (auth_suspend)));
  g_hash_table_insert (hash, g_strdup ("auth-hibernate"), g_strdup (espm_bool_to_string (auth_hibernate)));
  g_hash_table_insert (hash, g_strdup ("can-suspend"), g_strdup (espm_bool_to_string (can_suspend)));
  g_hash_table_insert (hash, g_strdup ("can-hibernate"), g_strdup (espm_bool_to_string (can_hibernate)));
  g_hash_table_insert (hash, g_strdup ("can-shutdown"), g_strdup (espm_bool_to_string (can_shutdown)));

  g_hash_table_insert (hash, g_strdup ("has-battery"), g_strdup (espm_bool_to_string (has_battery)));
  g_hash_table_insert (hash, g_strdup ("has-lid"), g_strdup (espm_bool_to_string (has_lid)));

  g_hash_table_insert (hash, g_strdup ("has-brightness"), g_strdup (espm_bool_to_string (has_lcd_brightness)));

  return hash;
}

/*
 *
 * DBus server implementation
 *
 */
static gboolean espm_manager_dbus_quit       (EspmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

static gboolean espm_manager_dbus_restart     (EspmManager *manager,
                                               GDBusMethodInvocation *invocation,
                                               gpointer user_data);

static gboolean espm_manager_dbus_get_config (EspmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

static gboolean espm_manager_dbus_get_info   (EspmManager *manager,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

#include "expidus-power-manager-dbus.h"

static void
espm_manager_dbus_class_init (EspmManagerClass *klass)
{
}

static void
espm_manager_dbus_init (EspmManager *manager)
{
  EspmPowerManager *manager_dbus;
  manager_dbus = espm_power_manager_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (manager_dbus),
                                    manager->priv->session_bus,
                                    "/org/expidus/PowerManager",
                                    NULL);

  g_signal_connect_swapped (manager_dbus,
                            "handle-quit",
                            G_CALLBACK (espm_manager_dbus_quit),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-restart",
                            G_CALLBACK (espm_manager_dbus_restart),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-get-config",
                            G_CALLBACK (espm_manager_dbus_get_config),
                            manager);
  g_signal_connect_swapped (manager_dbus,
                            "handle-get-info",
                            G_CALLBACK (espm_manager_dbus_get_info),
                            manager);
}

static gboolean
espm_manager_dbus_quit (EspmManager *manager,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data)
{
  ESPM_DEBUG("Quit message received\n");

  espm_manager_quit (manager);

  espm_power_manager_complete_quit (user_data, invocation);

  return TRUE;
}

static gboolean
espm_manager_dbus_restart (EspmManager *manager,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data)
{
  ESPM_DEBUG("Restart message received");

  espm_manager_quit (manager);

  g_spawn_command_line_async ("expidus1-power-manager", NULL);

  espm_power_manager_complete_restart (user_data, invocation);

  return TRUE;
}

static void hash_to_variant (gpointer key, gpointer value, gpointer user_data)
{
  g_variant_builder_add (user_data, "{ss}", key, value);
}

static gboolean
espm_manager_dbus_get_config (EspmManager *manager,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
  GHashTable *config;
  GVariantBuilder builder;
  GVariant *variant;

  config = espm_manager_get_config (manager);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));

  g_hash_table_foreach (config, hash_to_variant, &builder);

  g_hash_table_unref (config);

  variant = g_variant_builder_end (&builder);

  espm_power_manager_complete_get_config (user_data,
                                      invocation,
                                      variant);

  return TRUE;
}

static gboolean
espm_manager_dbus_get_info (EspmManager *manager,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{

  espm_power_manager_complete_get_info (user_data,
                                        invocation,
                                        PACKAGE,
                                        VERSION,
                                        "Expidus-goodies");

  return TRUE;
}
