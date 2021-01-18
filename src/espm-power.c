/*
 * * Copyright (C) 2009-2011 Ali <aliov@expidus.org>
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <upower.h>
#include <gdk/gdkx.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include "espm-power.h"
#include "espm-dbus.h"
#include "espm-dpms.h"
#include "espm-battery.h"
#include "espm-esconf.h"
#include "espm-notify.h"
#include "espm-errors.h"
#include "espm-console-kit.h"
#include "espm-inhibit.h"
#include "espm-polkit.h"
#include "espm-network-manager.h"
#include "espm-icons.h"
#include "espm-common.h"
#include "espm-power-common.h"
#include "espm-config.h"
#include "espm-debug.h"
#include "espm-enum-types.h"
#include "egg-idletime.h"
#include "espm-systemd.h"
#include "espm-suspend.h"
#include "espm-brightness.h"
#include "expidus-screensaver.h"

static void espm_power_finalize     (GObject *object);

static void espm_power_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec);

static void espm_power_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec);

static void espm_power_change_presentation_mode (EspmPower *power,
                                                 gboolean presentation_mode);

static void espm_update_blank_time (EspmPower *power);

static void espm_power_dbus_class_init (EspmPowerClass * klass);
static void espm_power_dbus_init (EspmPower *power);

struct EspmPowerPrivate
{
  GDBusConnection  *bus;

  UpClient         *upower;

  GHashTable       *hash;


  EspmSystemd      *systemd;
  EspmConsoleKit   *console;
  EspmInhibit      *inhibit;
  EspmEsconf       *conf;

  EspmBatteryCharge overall_state;
  gboolean          critical_action_done;

  EspmDpms         *dpms;
  gboolean          presentation_mode;
  gint              on_ac_blank;
  gint              on_battery_blank;
  EggIdletime      *idletime;

  gboolean          inhibited;
  gboolean          screensaver_inhibited;
  ExpidusScreenSaver  *screensaver;

  EspmNotify       *notify;
#ifdef ENABLE_POLKIT
  EspmPolkit       *polkit;
#endif
  gboolean          auth_suspend;
  gboolean          auth_hibernate;

  /* Properties */
  gboolean          on_low_battery;
  gboolean          lid_is_present;
  gboolean          lid_is_closed;
  gboolean          on_battery;
  gchar            *daemon_version;
  gboolean          can_suspend;
  gboolean          can_hibernate;

  /**
   * Warning dialog to use when notification daemon
   * doesn't support actions.
   **/
  GtkWidget        *dialog;
};

enum
{
  PROP_0,
  PROP_ON_LOW_BATTERY,
  PROP_ON_BATTERY,
  PROP_AUTH_SUSPEND,
  PROP_AUTH_HIBERNATE,
  PROP_CAN_SUSPEND,
  PROP_CAN_HIBERNATE,
  PROP_HAS_LID,
  PROP_PRESENTATION_MODE,
  PROP_ON_AC_BLANK,
  PROP_ON_BATTERY_BLANK,
  N_PROPERTIES
};

enum
{
  ON_BATTERY_CHANGED,
  LOW_BATTERY_CHANGED,
  LID_CHANGED,
  WAKING_UP,
  SLEEPING,
  ASK_SHUTDOWN,
  SHUTDOWN,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (EspmPower, espm_power, G_TYPE_OBJECT)


/* This checks if consolekit returns TRUE for either suspend or
 * hibernate showing support. This means that ConsoleKit2 is running
 * (and the system is capable of those actions).
 */
static gboolean
check_for_consolekit2 (EspmPower *power)
{
  EspmConsoleKit *console;
  gboolean can_suspend, can_hibernate;

  g_return_val_if_fail (ESPM_IS_POWER (power), FALSE);

  if (power->priv->console == NULL)
    return FALSE;

  console = power->priv->console;

  g_object_get (G_OBJECT (console),
                "can-suspend", &can_suspend,
                NULL);
  g_object_get (G_OBJECT (console),
                "can-hibernate", &can_hibernate,
                NULL);

    /* ConsoleKit2 supports suspend and hibernate */
  if (can_suspend || can_hibernate)
  {
    return TRUE;
  }

  return FALSE;
}

#ifdef ENABLE_POLKIT
static void
espm_power_check_polkit_auth (EspmPower *power)
{
  const char *suspend = NULL, *hibernate = NULL;
  if (LOGIND_RUNNING())
  {
    ESPM_DEBUG ("using logind suspend backend");
    suspend   = POLKIT_AUTH_SUSPEND_LOGIND;
    hibernate = POLKIT_AUTH_HIBERNATE_LOGIND;
  }
  else
  {
    if (power->priv->console != NULL)
    {
      /* ConsoleKit2 supports suspend and hibernate */
      if (check_for_consolekit2 (power))
      {
        ESPM_DEBUG ("using consolekit2 suspend backend");
        suspend   = POLKIT_AUTH_SUSPEND_CONSOLEKIT2;
        hibernate = POLKIT_AUTH_HIBERNATE_CONSOLEKIT2;
      }
      else
      {
        ESPM_DEBUG ("using espm internal suspend backend");
        suspend   = POLKIT_AUTH_SUSPEND_ESPM;
        hibernate = POLKIT_AUTH_HIBERNATE_ESPM;
      }
    }
  }
  power->priv->auth_suspend = espm_polkit_check_auth (power->priv->polkit,
                                                      suspend);

  power->priv->auth_hibernate = espm_polkit_check_auth (power->priv->polkit,
                                                        hibernate);
}
#endif

static void
espm_power_check_power (EspmPower *power, gboolean on_battery)
{
  if (on_battery != power->priv->on_battery )
  {
    GList *list;
    guint len, i;
    g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, on_battery);

    espm_dpms_set_on_battery (power->priv->dpms, on_battery);

      /* Dismiss critical notifications on battery state changes */
    espm_notify_close_critical (power->priv->notify);

    power->priv->on_battery = on_battery;
    list = g_hash_table_get_values (power->priv->hash);
    len = g_list_length (list);
    for ( i = 0; i < len; i++)
    {
      g_object_set (G_OBJECT (g_list_nth_data (list, i)),
                    "ac-online", !on_battery,
                    NULL);
      espm_update_blank_time (power);
    }
  }
}

static void
espm_power_check_lid (EspmPower *power, gboolean present, gboolean closed)
{
  power->priv->lid_is_present = present;

  if (power->priv->lid_is_present)
  {
    if (closed != power->priv->lid_is_closed )
    {
      power->priv->lid_is_closed = closed;
      g_signal_emit (G_OBJECT (power), signals [LID_CHANGED], 0, power->priv->lid_is_closed);
    }
  }
}

/*
 * Get the properties on org.freedesktop.DeviceKit.Power
 *
 * DaemonVersion      's'
 * CanSuspend'        'b'
 * CanHibernate'      'b'
 * OnBattery'         'b'
 * OnLowBattery'      'b'
 * LidIsClosed'       'b'
 * LidIsPresent'      'b'
 */
static void
espm_power_get_properties (EspmPower *power)
{
  gboolean on_battery;
  gboolean lid_is_closed;
  gboolean lid_is_present;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-suspend", &power->priv->can_suspend,
                  NULL);
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-hibernate", &power->priv->can_hibernate,
                  NULL);
  }
  else
  {
    if (check_for_consolekit2 (power))
    {
      g_object_get (G_OBJECT (power->priv->console),
        "can-suspend", &power->priv->can_suspend,
        NULL);
      g_object_get (G_OBJECT (power->priv->console),
        "can-hibernate", &power->priv->can_hibernate,
        NULL);
      }
    else
    {
      power->priv->can_suspend   = espm_suspend_can_suspend ();
      power->priv->can_hibernate = espm_suspend_can_hibernate ();
    }
  }

  g_object_get (power->priv->upower,
                "on-battery", &on_battery,
                "lid-is-closed", &lid_is_closed,
                "lid-is-present", &lid_is_present,
                NULL);
  espm_power_check_lid (power, lid_is_present, lid_is_closed);
  espm_power_check_power (power, on_battery);
}

static void
espm_power_report_error (EspmPower *power, const gchar *error, const gchar *icon_name)
{
  GtkStatusIcon *battery = NULL;
  guint i, len;
  GList *list;

  list = g_hash_table_get_values (power->priv->hash);
  len = g_list_length (list);

  for ( i = 0; i < len; i++)
  {
    UpDeviceKind type;
    battery = g_list_nth_data (list, i);
    type = espm_battery_get_device_type (ESPM_BATTERY (battery));
    if ( type == UP_DEVICE_KIND_BATTERY ||
         type == UP_DEVICE_KIND_UPS )
      break;
  }

  espm_notify_show_notification (power->priv->notify,
                                 _("Power Manager"),
                                 error,
                                 icon_name,
                                 ESPM_NOTIFY_CRITICAL);
}

static void
espm_power_sleep (EspmPower *power, const gchar *sleep_time, gboolean force)
{
  GError *error = NULL;
  gboolean lock_screen;
#ifdef WITH_NETWORK_MANAGER
  gboolean network_manager_sleep;
#endif
  EspmBrightness *brightness;
  gint32 brightness_level;

  if ( power->priv->inhibited && force == FALSE)
  {
    GtkWidget *dialog;
    gboolean ret;

    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("An application is currently disabling the automatic sleep. "
                                       "Doing this action now may damage the working state of this application.\n"
                                       "Are you sure you want to hibernate the system?"));
  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if ( !ret || ret == GTK_RESPONSE_NO)
    return;
  }

  g_signal_emit (G_OBJECT (power), signals [SLEEPING], 0);
    /* Get the current brightness level so we can use it after we suspend */
  brightness = espm_brightness_new();
  espm_brightness_setup (brightness);
  espm_brightness_get_level (brightness, &brightness_level);

#ifdef WITH_NETWORK_MANAGER
  g_object_get (G_OBJECT (power->priv->conf),
                NETWORK_MANAGER_SLEEP, &network_manager_sleep,
                NULL);

  if ( network_manager_sleep )
  {
    espm_network_manager_sleep (TRUE);
  }
#endif

  g_object_get (G_OBJECT (power->priv->conf),
                LOCK_SCREEN_ON_SLEEP, &lock_screen,
                NULL);

  if ( lock_screen )
  {
#ifdef WITH_NETWORK_MANAGER
    if ( network_manager_sleep )
    {
  /* 2 seconds, to give network manager time to sleep */
      g_usleep (2000000);
    }
#endif
    if (!expidus_screensaver_lock (power->priv->screensaver))
    {
      GtkWidget *dialog;
      gboolean ret;

      dialog = gtk_message_dialog_new (NULL,
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_YES_NO,
                                       _("None of the screen lock tools ran "
                                         "successfully, the screen will not "
                                         "be locked.\n"
                                         "Do you still want to continue to "
                                         "suspend the system?"));
      ret = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if ( !ret || ret == GTK_RESPONSE_NO)
        return;
      }
    }

    /* This is fun, here's the order of operations:
     * - if the Logind is running then use it
     * - if UPower < 0.99.0 then use it (don't make changes on the user unless forced)
     * - if ConsoleKit2 is running then use it
     * - if everything else fails use our built-in fallback
     */
  if ( LOGIND_RUNNING () )
  {
    espm_systemd_sleep (power->priv->systemd, sleep_time, &error);
  }
  else
  {
    if (!g_strcmp0 (sleep_time, "Hibernate"))
    {
      if (check_for_consolekit2 (power))
      {
        espm_console_kit_hibernate (power->priv->console, &error);
      }
      else
      {
        espm_suspend_try_action (ESPM_HIBERNATE);
      }
    }
    else
    {
      if (check_for_consolekit2 (power))
      {
        espm_console_kit_suspend (power->priv->console, &error);
      }
      else
      {
        espm_suspend_try_action (ESPM_SUSPEND);
      }
    }
  }

  if ( error )
  {
    if ( g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY) )
    {
      ESPM_DEBUG ("D-Bus time out, but should be harmless");
    }
    else
    {
      espm_power_report_error (power, error->message, "dialog-error");
      g_error_free (error);
    }
  }

  g_signal_emit (G_OBJECT (power), signals [WAKING_UP], 0);
    /* Check/update any changes while we slept */
  espm_power_get_properties (power);
    /* Restore the brightness level from before we suspended */
  espm_brightness_set_level (brightness, brightness_level);

#ifdef WITH_NETWORK_MANAGER
  if ( network_manager_sleep )
  {
    espm_network_manager_sleep (FALSE);
  }
#endif
}

static void
espm_power_hibernate_clicked (EspmPower *power)
{
  gtk_widget_destroy (power->priv->dialog );
  power->priv->dialog = NULL;
  espm_power_sleep (power, "Hibernate", TRUE);
}

static void
espm_power_suspend_clicked (EspmPower *power)
{
  gtk_widget_destroy (power->priv->dialog );
  power->priv->dialog = NULL;
  espm_power_sleep (power, "Suspend", TRUE);
}

static void
espm_power_shutdown_clicked (EspmPower *power)
{
  gtk_widget_destroy (power->priv->dialog );
  power->priv->dialog = NULL;
  g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
}

static EspmBatteryCharge
espm_power_get_current_charge_state (EspmPower *power)
{
  GList *list;
  guint len, i;
  gboolean power_supply;
  EspmBatteryCharge max_charge_status = ESPM_BATTERY_CHARGE_UNKNOWN;

  list = g_hash_table_get_values (power->priv->hash);
  len = g_list_length (list);

  for ( i = 0; i < len; i++ )
  {
    EspmBatteryCharge battery_charge;
    UpDeviceKind type;

    g_object_get (G_OBJECT (g_list_nth_data (list, i)),
                    "charge-status", &battery_charge,
                    "device-type", &type,
                    "ac-online", &power_supply,
                    NULL);
    if ( type != UP_DEVICE_KIND_BATTERY &&
         type != UP_DEVICE_KIND_UPS &&
         power_supply != TRUE)
      continue;

    max_charge_status = MAX (max_charge_status, battery_charge);
  }

  return max_charge_status;
}

static void
espm_power_notify_action_callback (NotifyNotification *n, gchar *action, EspmPower *power)
{
  if ( !g_strcmp0 (action, "Shutdown") )
    g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
  else
    espm_power_sleep (power, action, TRUE);
}

static void
espm_power_add_actions_to_notification (EspmPower *power, NotifyNotification *n)
{
  gboolean can_shutdown;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }

  if ( power->priv->can_hibernate && power->priv->auth_hibernate )
  {
    espm_notify_add_action_to_notification(
         power->priv->notify,
         n,
         "Hibernate",
         _("Hibernate the system"),
         (NotifyActionCallback)espm_power_notify_action_callback,
         power);
  }

  if ( power->priv->can_suspend && power->priv->auth_suspend )
  {
    espm_notify_add_action_to_notification(
         power->priv->notify,
         n,
         "Suspend",
         _("Suspend the system"),
         (NotifyActionCallback)espm_power_notify_action_callback,
         power);
  }

  if ( can_shutdown )
  espm_notify_add_action_to_notification (power->priv->notify,
                                          n,
                                          "Shutdown",
                                          _("Shutdown the system"),
                                          (NotifyActionCallback)espm_power_notify_action_callback,
                                          power);
}

static void
espm_power_show_critical_action_notification (EspmPower *power, EspmBattery *battery)
{
  const gchar *message;
  NotifyNotification *n;

  message = _("System is running on low power. "\
               "Save your work to avoid losing data");

  n =
  espm_notify_new_notification (power->priv->notify,
                                _("Power Manager"),
                                message,
                                espm_battery_get_icon_name (battery),
                                ESPM_NOTIFY_CRITICAL);

  espm_power_add_actions_to_notification (power, n);
  espm_notify_critical (power->priv->notify, n);

}

static void
espm_power_close_critical_dialog (EspmPower *power)
{
  gtk_widget_destroy (power->priv->dialog);
  power->priv->dialog = NULL;
}

static void
espm_power_show_critical_action_gtk (EspmPower *power)
{
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *cancel;
  const gchar *message;
  gboolean can_shutdown;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }

  message = _("System is running on low power. "\
              "Save your work to avoid losing data");

  dialog = gtk_dialog_new_with_buttons (_("Power Manager"), NULL, GTK_DIALOG_MODAL,
                                        NULL, NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_CANCEL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  gtk_box_pack_start (GTK_BOX (content_area), gtk_label_new (message),
                      TRUE, TRUE, 8);

  if ( power->priv->can_hibernate && power->priv->auth_hibernate )
  {
    GtkWidget *hibernate;
    hibernate = gtk_button_new_with_label (_("Hibernate"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), hibernate, GTK_RESPONSE_NONE);

    g_signal_connect_swapped (hibernate, "clicked",
                              G_CALLBACK (espm_power_hibernate_clicked), power);
  }

  if ( power->priv->can_suspend && power->priv->auth_suspend )
  {
    GtkWidget *suspend;

    suspend = gtk_button_new_with_label (_("Suspend"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), suspend, GTK_RESPONSE_NONE);

    g_signal_connect_swapped (suspend, "clicked",
                              G_CALLBACK (espm_power_suspend_clicked), power);
    }

  if ( can_shutdown )
  {
    GtkWidget *shutdown;

    shutdown = gtk_button_new_with_label (_("Shutdown"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), shutdown, GTK_RESPONSE_NONE);

    g_signal_connect_swapped (shutdown, "clicked",
                              G_CALLBACK (espm_power_shutdown_clicked), power);
  }

  cancel = gtk_button_new_with_label (_("_Cancel"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancel, GTK_RESPONSE_NONE);

  g_signal_connect_swapped (cancel, "clicked",
                            G_CALLBACK (espm_power_close_critical_dialog), power);

  g_signal_connect_swapped (dialog, "destroy",
                            G_CALLBACK (espm_power_close_critical_dialog), power);
  if ( power->priv->dialog )
  {
    gtk_widget_destroy (power->priv->dialog);
    power->priv->dialog = NULL;
  }
  power->priv->dialog = dialog;
  gtk_widget_show_all (dialog);
}

static void
espm_power_show_critical_action (EspmPower *power, EspmBattery *battery)
{
  gboolean supports_actions;

  g_object_get (G_OBJECT (power->priv->notify),
                "actions", &supports_actions,
                NULL);

  if ( supports_actions )
    espm_power_show_critical_action_notification (power, battery);
  else
    espm_power_show_critical_action_gtk (power);
}

static void
espm_power_process_critical_action (EspmPower *power, EspmShutdownRequest req)
{
  if ( req == ESPM_ASK )
    g_signal_emit (G_OBJECT (power), signals [ASK_SHUTDOWN], 0);
  else if ( req == ESPM_DO_SUSPEND )
    espm_power_sleep (power, "Suspend", TRUE);
  else if ( req == ESPM_DO_HIBERNATE )
    espm_power_sleep (power, "Hibernate", TRUE);
  else if ( req == ESPM_DO_SHUTDOWN )
    g_signal_emit (G_OBJECT (power), signals [SHUTDOWN], 0);
}

static void
espm_power_system_on_critical_power (EspmPower *power, EspmBattery *battery)
{
  EspmShutdownRequest critical_action;

  g_object_get (G_OBJECT (power->priv->conf),
                CRITICAL_BATT_ACTION_CFG, &critical_action,
                NULL);

  ESPM_DEBUG ("System is running on low power");
  ESPM_DEBUG_ENUM (critical_action, ESPM_TYPE_SHUTDOWN_REQUEST, "Critical battery action");

  if ( critical_action == ESPM_DO_NOTHING )
  {
    espm_power_show_critical_action (power, battery);
  }
  else
  {
    if (power->priv->critical_action_done == FALSE)
    {
      power->priv->critical_action_done = TRUE;
      espm_power_process_critical_action (power, critical_action);
    }
    else
    {
      espm_power_show_critical_action (power, battery);
    }
  }
}

static void
espm_power_battery_charge_changed_cb (EspmBattery *battery, EspmPower *power)
{
  gboolean notify;
  EspmBatteryCharge battery_charge;
  EspmBatteryCharge current_charge;

  battery_charge = espm_battery_get_charge (battery);
  current_charge = espm_power_get_current_charge_state (power);

  ESPM_DEBUG_ENUM (current_charge, ESPM_TYPE_BATTERY_CHARGE, "Current system charge status");

  if (current_charge == power->priv->overall_state)
    return;

  if (current_charge >= ESPM_BATTERY_CHARGE_LOW)
    power->priv->critical_action_done = FALSE;

  power->priv->overall_state = current_charge;

  if ( current_charge == ESPM_BATTERY_CHARGE_CRITICAL && power->priv->on_battery)
  {
    espm_power_system_on_critical_power (power, battery);

    power->priv->on_low_battery = TRUE;
    g_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
    return;
  }

  if ( power->priv->on_low_battery )
  {
    power->priv->on_low_battery = FALSE;
    g_signal_emit (G_OBJECT (power), signals [LOW_BATTERY_CHANGED], 0, power->priv->on_low_battery);
  }

  g_object_get (G_OBJECT (power->priv->conf),
                GENERAL_NOTIFICATION_CFG, &notify,
                NULL);

  if ( power->priv->on_battery )
  {
    if ( current_charge == ESPM_BATTERY_CHARGE_LOW )
    {
      if ( notify )
        espm_notify_show_notification (power->priv->notify,
                       _("Power Manager"),
                       _("System is running on low power"),
                       espm_battery_get_icon_name (battery),
                       ESPM_NOTIFY_NORMAL);

     }
    else if ( battery_charge == ESPM_BATTERY_CHARGE_LOW )
    {
      if ( notify )
      {
        gchar *msg;
        gchar *time_str;

        const gchar *battery_name = espm_battery_get_battery_name (battery);

        time_str = espm_battery_get_time_left (battery);

        msg = g_strdup_printf (_("Your %s charge level is low\nEstimated time left %s"), battery_name, time_str);


        espm_notify_show_notification (power->priv->notify,
                       _("Power Manager"),
                       msg,
                       espm_battery_get_icon_name (battery),
                       ESPM_NOTIFY_NORMAL);
        g_free (msg);
        g_free (time_str);
      }
    }
  }

    /*Current charge is okay now, then close the dialog*/
  if ( power->priv->dialog )
  {
    gtk_widget_destroy (power->priv->dialog);
    power->priv->dialog = NULL;
  }
}

static void
espm_power_add_device (UpDevice *device, EspmPower *power)
{
  guint device_type = UP_DEVICE_KIND_UNKNOWN;
  const gchar *object_path = up_device_get_object_path(device);

    /* hack, this depends on ESPM_DEVICE_TYPE_* being in sync with UP_DEVICE_KIND_* */
  g_object_get (device,
                "kind", &device_type,
                NULL);

  ESPM_DEBUG ("'%s' device added", up_device_kind_to_string(device_type));

  if ( device_type == UP_DEVICE_KIND_BATTERY  ||
       device_type == UP_DEVICE_KIND_UPS  ||
       device_type == UP_DEVICE_KIND_MOUSE  ||
       device_type == UP_DEVICE_KIND_KEYBOARD ||
       device_type == UP_DEVICE_KIND_PHONE)
  {
    GtkWidget *battery;
    ESPM_DEBUG( "Battery device type '%s' detected at: %s",
                up_device_kind_to_string(device_type), object_path);
    battery = espm_battery_new ();

    espm_battery_monitor_device (ESPM_BATTERY (battery),
                                 object_path,
                                 device_type);
    g_hash_table_insert (power->priv->hash, g_strdup (object_path), battery);

    g_signal_connect (battery, "battery-charge-changed",
                      G_CALLBACK (espm_power_battery_charge_changed_cb), power);
  }
}

static void
espm_power_get_power_devices (EspmPower *power)
{
  GPtrArray *array = NULL;
  guint i;

#if UP_CHECK_VERSION(0, 99, 8)
  array = up_client_get_devices2 (power->priv->upower);
#else
  array = up_client_get_devices (power->priv->upower);
#endif

  if ( array )
  {
    for ( i = 0; i < array->len; i++)
    {
      UpDevice *device = g_ptr_array_index (array, i);
      const gchar *object_path = up_device_get_object_path(device);
      ESPM_DEBUG ("Power device detected at : %s", object_path);
      espm_power_add_device (device, power);
    }
  g_ptr_array_free (array, TRUE);
  }
}

static void
espm_power_remove_device (EspmPower *power, const gchar *object_path)
{
  g_hash_table_remove (power->priv->hash, object_path);
}

static void
espm_power_inhibit_changed_cb (EspmInhibit *inhibit, gboolean is_inhibit, EspmPower *power)
{
  if (power->priv->inhibited != is_inhibit)
  {
    power->priv->inhibited = is_inhibit;

    ESPM_DEBUG ("is_inhibit %s, screensaver_inhibited %s, presentation_mode %s",
                power->priv->inhibited ? "TRUE" : "FALSE",
                power->priv->screensaver_inhibited ? "TRUE" : "FALSE",
                power->priv->presentation_mode ? "TRUE" : "FALSE");

    /* If we are inhibited make sure we inhibit the screensaver too */
    if (is_inhibit)
    {
      if (!power->priv->screensaver_inhibited)
      {
        expidus_screensaver_inhibit (power->priv->screensaver, TRUE);
        power->priv->screensaver_inhibited = TRUE;
      }
    }
    else
    {
      /* Or make sure we remove the screensaver inhibit */
      if (power->priv->screensaver_inhibited && !power->priv->presentation_mode)
      {
        expidus_screensaver_inhibit (power->priv->screensaver, FALSE);
        power->priv->screensaver_inhibited = FALSE;
      }
    }
  }

  ESPM_DEBUG ("is_inhibit %s, screensaver_inhibited %s, presentation_mode %s",
  power->priv->inhibited ? "TRUE" : "FALSE",
  power->priv->screensaver_inhibited ? "TRUE" : "FALSE",
  power->priv->presentation_mode ? "TRUE" : "FALSE");
}

static void
espm_power_changed_cb (UpClient *upower,
                       GParamSpec *pspec,
                       EspmPower *power)
{
  espm_power_get_properties (power);
}

static void
espm_power_device_added_cb (UpClient *upower, UpDevice *device, EspmPower *power)
{
  espm_power_add_device (device, power);
}

static void
espm_power_device_removed_cb (UpClient *upower, const gchar *object_path, EspmPower *power)
{
  espm_power_remove_device (power, object_path);
}

#ifdef ENABLE_POLKIT
static void
espm_power_polkit_auth_changed_cb (EspmPower *power)
{
  ESPM_DEBUG ("Auth configuration changed");
  espm_power_check_polkit_auth (power);
}
#endif

static void
espm_power_class_init (EspmPowerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = espm_power_finalize;

  object_class->get_property = espm_power_get_property;
  object_class->set_property = espm_power_set_property;

  signals [ON_BATTERY_CHANGED] =
        g_signal_new ("on-battery-changed",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, on_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals [LOW_BATTERY_CHANGED] =
        g_signal_new ("low-battery-changed",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, low_battery_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals [LID_CHANGED] =
        g_signal_new ("lid-changed",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, lid_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals [WAKING_UP] =
        g_signal_new ("waking-up",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, waking_up),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

  signals [SLEEPING] =
        g_signal_new ("sleeping",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, sleeping),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

  signals [ASK_SHUTDOWN] =
        g_signal_new ("ask-shutdown",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, ask_shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

  signals [SHUTDOWN] =
        g_signal_new ("shutdown",
                      ESPM_TYPE_POWER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EspmPowerClass, shutdown),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0, G_TYPE_NONE);

#define ESPM_PARAM_FLAGS  (  G_PARAM_READWRITE \
                           | G_PARAM_CONSTRUCT \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)

  g_object_class_install_property (object_class,
                                   PROP_ON_BATTERY,
                                   g_param_spec_boolean ("on-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_ON_LOW_BATTERY,
                                   g_param_spec_boolean ("on-low-battery",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_AUTH_SUSPEND,
                                   g_param_spec_boolean ("auth-suspend",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_AUTH_HIBERNATE,
                                   g_param_spec_boolean ("auth-hibernate",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CAN_HIBERNATE,
                                   g_param_spec_boolean ("can-hibernate",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CAN_SUSPEND,
                                   g_param_spec_boolean ("can-suspend",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_HAS_LID,
                                   g_param_spec_boolean ("has-lid",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_PRESENTATION_MODE,
                                   g_param_spec_boolean (PRESENTATION_MODE,
                                                         NULL, NULL,
                                                         FALSE,
                                                         ESPM_PARAM_FLAGS));

  g_object_class_install_property (object_class,
                                   PROP_ON_AC_BLANK,
                                   g_param_spec_int (ON_AC_BLANK,
                                                    NULL, NULL,
                                                    0,
                                                    G_MAXINT16,
                                                    10,
                                                    ESPM_PARAM_FLAGS));

  g_object_class_install_property (object_class,
                                   PROP_ON_BATTERY_BLANK,
                                   g_param_spec_int (ON_BATTERY_BLANK,
                                                    NULL, NULL,
                                                    0,
                                                    G_MAXINT16,
                                                    10,
                                                    ESPM_PARAM_FLAGS));
#undef ESPM_PARAM_FLAGS

  espm_power_dbus_class_init (klass);
}

static void
espm_power_init (EspmPower *power)
{
  GError *error = NULL;

  power->priv = espm_power_get_instance_private (power);

  power->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  power->priv->lid_is_present  = FALSE;
  power->priv->lid_is_closed   = FALSE;
  power->priv->on_battery      = FALSE;
  power->priv->on_low_battery  = FALSE;
  power->priv->daemon_version  = NULL;
  power->priv->can_suspend     = FALSE;
  power->priv->can_hibernate   = FALSE;
  power->priv->auth_hibernate  = TRUE;
  power->priv->auth_suspend    = TRUE;
  power->priv->dialog          = NULL;
  power->priv->overall_state   = ESPM_BATTERY_CHARGE_OK;
  power->priv->critical_action_done = FALSE;

  power->priv->dpms                 = espm_dpms_new ();

  power->priv->presentation_mode    = FALSE;
  power->priv->on_ac_blank          = 15;
  power->priv->on_battery_blank     = 10;

  power->priv->inhibit = espm_inhibit_new ();
  power->priv->notify  = espm_notify_new ();
  power->priv->conf    = espm_esconf_new ();
  power->priv->upower  = up_client_new ();
  power->priv->screensaver = expidus_screensaver_new ();

  power->priv->systemd = NULL;
  power->priv->console = NULL;
  if ( LOGIND_RUNNING () )
    power->priv->systemd = espm_systemd_new ();
  else
    power->priv->console = espm_console_kit_new ();

#ifdef ENABLE_POLKIT
  power->priv->polkit  = espm_polkit_get ();
  g_signal_connect_swapped (power->priv->polkit, "auth-changed",
                            G_CALLBACK (espm_power_polkit_auth_changed_cb), power);
#endif

  g_signal_connect (power->priv->inhibit, "has-inhibit-changed",
                    G_CALLBACK (espm_power_inhibit_changed_cb), power);

  power->priv->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if ( error )
  {
    g_critical ("Unable to connect to the system bus : %s", error->message);
    g_error_free (error);
    goto out;
  }

  g_signal_connect (power->priv->upower, "device-added", G_CALLBACK (espm_power_device_added_cb), power);
  g_signal_connect (power->priv->upower, "device-removed", G_CALLBACK (espm_power_device_removed_cb), power);
  g_signal_connect (power->priv->upower, "notify", G_CALLBACK (espm_power_changed_cb), power);

  espm_power_get_power_devices (power);
  espm_power_get_properties (power);
#ifdef ENABLE_POLKIT
  espm_power_check_polkit_auth (power);
#endif

out:
  espm_power_dbus_init (power);

  /*
   * Emit org.freedesktop.PowerManagement session signals on startup
   */
  g_signal_emit (G_OBJECT (power), signals [ON_BATTERY_CHANGED], 0, power->priv->on_battery);
}

static void
espm_power_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  EspmPower *power;
  power = ESPM_POWER (object);

  switch (prop_id)
  {
    case PROP_ON_BATTERY:
      g_value_set_boolean (value, power->priv->on_battery);
      break;
    case PROP_AUTH_HIBERNATE:
      g_value_set_boolean (value, power->priv->auth_hibernate);
      break;
    case PROP_AUTH_SUSPEND:
      g_value_set_boolean (value, power->priv->auth_suspend);
      break;
    case PROP_CAN_SUSPEND:
      g_value_set_boolean (value, power->priv->can_suspend);
      break;
    case PROP_CAN_HIBERNATE:
      g_value_set_boolean (value, power->priv->can_hibernate);
      break;
    case PROP_HAS_LID:
      g_value_set_boolean (value, power->priv->lid_is_present);
      break;
    case PROP_PRESENTATION_MODE:
      g_value_set_boolean (value, power->priv->presentation_mode);
      break;
    case PROP_ON_AC_BLANK:
      g_value_set_int (value, power->priv->on_ac_blank);
      break;
    case PROP_ON_BATTERY_BLANK:
      g_value_set_int (value, power->priv->on_battery_blank);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_power_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  EspmPower *power = ESPM_POWER (object);
  gint on_ac_blank, on_battery_blank;

  switch (prop_id)
  {
    case PROP_PRESENTATION_MODE:
      espm_power_change_presentation_mode (power, g_value_get_boolean (value));
      break;
    case PROP_ON_AC_BLANK:
      on_ac_blank = g_value_get_int (value);
      power->priv->on_ac_blank = on_ac_blank;
      if (!power->priv->on_battery)
        espm_update_blank_time (power);
      break;
    case PROP_ON_BATTERY_BLANK:
      on_battery_blank = g_value_get_int (value);
      power->priv->on_battery_blank = on_battery_blank;
      if (power->priv->on_battery)
        espm_update_blank_time (power);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_power_finalize (GObject *object)
{
  EspmPower *power;

  power = ESPM_POWER (object);

  g_free (power->priv->daemon_version);

  g_object_unref (power->priv->inhibit);
  g_object_unref (power->priv->notify);
  g_object_unref (power->priv->conf);
  g_object_unref (power->priv->screensaver);

  if ( power->priv->systemd != NULL )
    g_object_unref (power->priv->systemd);
  if ( power->priv->console != NULL )
    g_object_unref (power->priv->console);

  g_object_unref (power->priv->bus);

  g_hash_table_destroy (power->priv->hash);

#ifdef ENABLE_POLKIT
  g_object_unref (power->priv->polkit);
#endif

  g_object_unref(power->priv->dpms);

  G_OBJECT_CLASS (espm_power_parent_class)->finalize (object);
}

static EspmPower*
espm_power_new (void)
{
  EspmPower *power = ESPM_POWER(g_object_new (ESPM_TYPE_POWER, NULL));

  esconf_g_property_bind (espm_esconf_get_channel(power->priv->conf),
                          ESPM_PROPERTIES_PREFIX PRESENTATION_MODE, G_TYPE_BOOLEAN,
                          G_OBJECT(power), PRESENTATION_MODE);

  esconf_g_property_bind (espm_esconf_get_channel(power->priv->conf),
                          ESPM_PROPERTIES_PREFIX ON_BATTERY_BLANK, G_TYPE_INT,
                          G_OBJECT (power), ON_BATTERY_BLANK);

  esconf_g_property_bind (espm_esconf_get_channel(power->priv->conf),
                          ESPM_PROPERTIES_PREFIX ON_AC_BLANK, G_TYPE_INT,
                          G_OBJECT (power), ON_AC_BLANK);

  return power;
}

EspmPower *
espm_power_get (void)
{
  static gpointer espm_power_object = NULL;

  if ( G_LIKELY (espm_power_object != NULL ) )
  {
    g_object_ref (espm_power_object);
  }
  else
  {
    espm_power_object = espm_power_new ();
    g_object_add_weak_pointer (espm_power_object, &espm_power_object);
  }

  return ESPM_POWER (espm_power_object);
}

void espm_power_suspend (EspmPower *power, gboolean force)
{
  espm_power_sleep (power, "Suspend", force);
}

void espm_power_hibernate (EspmPower *power, gboolean force)
{
  espm_power_sleep (power, "Hibernate", force);
}

gboolean espm_power_has_battery (EspmPower *power)
{
  GtkStatusIcon *battery = NULL;
  guint i, len;
  GList *list;

  gboolean ret = FALSE;

  list = g_hash_table_get_values (power->priv->hash);
  len = g_list_length (list);

  for ( i = 0; i < len; i++)
  {
    UpDeviceKind type;
    battery = g_list_nth_data (list, i);
    type = espm_battery_get_device_type (ESPM_BATTERY (battery));
    if ( type == UP_DEVICE_KIND_BATTERY ||
         type == UP_DEVICE_KIND_UPS )
    {
      ret = TRUE;
      break;
    }
  }

  return ret;
}

static void
espm_update_blank_time (EspmPower *power)
{
  int prev_timeout, prev_interval, prev_prefer_blanking, prev_allow_exposures;
  Display* display = gdk_x11_display_get_xdisplay(gdk_display_get_default ());
  guint screensaver_timeout;

  if (power->priv->on_battery)
    screensaver_timeout = power->priv->on_battery_blank;
  else
    screensaver_timeout = power->priv->on_ac_blank;

    /* Presentation mode disables blanking */
  if (power->priv->presentation_mode)
    screensaver_timeout = 0;

  screensaver_timeout = screensaver_timeout * 60;

  XGetScreenSaver(display, &prev_timeout, &prev_interval, &prev_prefer_blanking, &prev_allow_exposures);
  ESPM_DEBUG ("Prev Timeout: %d / New Timeout: %d", prev_timeout, screensaver_timeout);
  XSetScreenSaver(display, screensaver_timeout, prev_interval, prev_prefer_blanking, prev_allow_exposures);
  XSync (display, FALSE);
}

static void
espm_power_change_presentation_mode (EspmPower *power, gboolean presentation_mode)
{
    /* no change, exit */
  if (power->priv->presentation_mode == presentation_mode)
    return;

  power->priv->presentation_mode = presentation_mode;

  /* presentation mode inhibits dpms */
  espm_dpms_inhibit (power->priv->dpms, presentation_mode);

  ESPM_DEBUG ("is_inhibit %s, screensaver_inhibited %s, presentation_mode %s",
  power->priv->inhibited ? "TRUE" : "FALSE",
  power->priv->screensaver_inhibited ? "TRUE" : "FALSE",
  power->priv->presentation_mode ? "TRUE" : "FALSE");

  if (presentation_mode)
  {
  /* presentation mode inhibits the screensaver */
    if (!power->priv->screensaver_inhibited)
    {
      expidus_screensaver_inhibit (power->priv->screensaver, TRUE);
      power->priv->screensaver_inhibited = TRUE;
    }
  }
  else
  {
    EggIdletime *idletime;

    /* make sure we remove the screensaver inhibit */
    if (power->priv->screensaver_inhibited && !power->priv->inhibited)
    {
      expidus_screensaver_inhibit (power->priv->screensaver, FALSE);
      power->priv->screensaver_inhibited = FALSE;
    }

    /* reset the timers */
    idletime = egg_idletime_new ();
    egg_idletime_alarm_reset_all (idletime);

    g_object_unref (idletime);
  }

  ESPM_DEBUG ("is_inhibit %s, screensaver_inhibited %s, presentation_mode %s",
  power->priv->inhibited ? "TRUE" : "FALSE",
  power->priv->screensaver_inhibited ? "TRUE" : "FALSE",
  power->priv->presentation_mode ? "TRUE" : "FALSE");

  espm_update_blank_time (power);
}

gboolean
espm_power_is_in_presentation_mode (EspmPower *power)
{
  g_return_val_if_fail (ESPM_IS_POWER (power), FALSE);

  return power->priv->presentation_mode || power->priv->inhibited;
}


/*
 *
 * DBus server implementation for org.freedesktop.PowerManagement
 *
 */
static gboolean espm_power_dbus_shutdown (EspmPower *power,
                                          GDBusMethodInvocation *invocation,
                                          gpointer user_data);

static gboolean espm_power_dbus_reboot   (EspmPower *power,
                                          GDBusMethodInvocation *invocation,
                                          gpointer user_data);

static gboolean espm_power_dbus_hibernate (EspmPower * power,
                                           GDBusMethodInvocation *invocation,
                                           gpointer user_data);

static gboolean espm_power_dbus_suspend (EspmPower * power,
                                         GDBusMethodInvocation *invocation,
                                         gpointer user_data);

static gboolean espm_power_dbus_can_reboot (EspmPower * power,
                                            GDBusMethodInvocation *invocation,
                                            gpointer user_data);

static gboolean espm_power_dbus_can_shutdown (EspmPower * power,
                                              GDBusMethodInvocation *invocation,
                                              gpointer user_data);

static gboolean espm_power_dbus_can_hibernate (EspmPower * power,
                                               GDBusMethodInvocation *invocation,
                                               gpointer user_data);

static gboolean espm_power_dbus_can_suspend (EspmPower * power,
                                             GDBusMethodInvocation *invocation,
                                             gpointer user_data);

static gboolean espm_power_dbus_get_on_battery (EspmPower * power,
                                                GDBusMethodInvocation *invocation,
                                                gpointer user_data);

static gboolean espm_power_dbus_get_low_battery (EspmPower * power,
                                                 GDBusMethodInvocation *invocation,
                                                 gpointer user_data);

#include "org.freedesktop.PowerManagement.h"

static void
espm_power_dbus_class_init (EspmPowerClass * klass)
{
}

static void
espm_power_dbus_init (EspmPower *power)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  EspmPowerManagement *power_dbus;

  TRACE ("entering");

  power_dbus = espm_power_management_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (power_dbus),
                                    bus,
                                    "/org/freedesktop/PowerManagement",
                                    NULL);

  g_signal_connect_swapped (power_dbus,
                            "handle-shutdown",
                            G_CALLBACK (espm_power_dbus_shutdown),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-reboot",
                            G_CALLBACK (espm_power_dbus_reboot),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-hibernate",
                            G_CALLBACK (espm_power_dbus_hibernate),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-suspend",
                            G_CALLBACK (espm_power_dbus_suspend),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-can-reboot",
                            G_CALLBACK (espm_power_dbus_can_reboot),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-can-shutdown",
                            G_CALLBACK (espm_power_dbus_can_shutdown),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-can-hibernate",
                            G_CALLBACK (espm_power_dbus_can_hibernate),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-can-suspend",
                            G_CALLBACK (espm_power_dbus_can_suspend),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-get-on-battery",
                            G_CALLBACK (espm_power_dbus_get_on_battery),
                            power);
  g_signal_connect_swapped (power_dbus,
                            "handle-get-low-battery",
                            G_CALLBACK (espm_power_dbus_get_low_battery),
                            power);
}

static gboolean espm_power_dbus_shutdown (EspmPower *power,
            GDBusMethodInvocation *invocation,
            gpointer user_data)
{
  GError *error = NULL;
  gboolean can_reboot;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-shutdown", &can_reboot,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-shutdown", &can_reboot,
                  NULL);
  }

  if ( !can_reboot)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if ( LOGIND_RUNNING () )
    espm_systemd_shutdown (power->priv->systemd, &error);
  else
    espm_console_kit_shutdown (power->priv->console, &error);

  if (error)
  {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }
  else
  {
    espm_power_management_complete_shutdown (user_data, invocation);
  }

  return TRUE;
}

static gboolean
espm_power_dbus_reboot   (EspmPower *power,
                          GDBusMethodInvocation *invocation,
                          gpointer user_data)
{
  GError *error = NULL;
  gboolean can_reboot;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-restart", &can_reboot,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-restart", &can_reboot,
                  NULL);
  }

  if ( !can_reboot)
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if ( LOGIND_RUNNING () )
    espm_systemd_reboot (power->priv->systemd, &error);
  else
    espm_console_kit_reboot (power->priv->console, &error);

  if (error)
  {
    g_dbus_method_invocation_return_gerror (invocation, error);
    g_error_free (error);
  }
  else
  {
    espm_power_management_complete_reboot (user_data, invocation);
  }

  return TRUE;
}

static gboolean
espm_power_dbus_hibernate (EspmPower * power,
                           GDBusMethodInvocation *invocation,
                           gpointer user_data)
{
  if ( !power->priv->auth_hibernate )
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (!power->priv->can_hibernate )
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_NO_HARDWARE_SUPPORT,
                                           _("Suspend not supported"));
    return TRUE;
  }

  espm_power_sleep (power, "Hibernate", FALSE);

  espm_power_management_complete_hibernate (user_data, invocation);

  return TRUE;
}

static gboolean
espm_power_dbus_suspend (EspmPower * power,
                         GDBusMethodInvocation *invocation,
                         gpointer user_data)
{
  if ( !power->priv->auth_suspend )
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_PERMISSION_DENIED,
                                           _("Permission denied"));
    return TRUE;
  }

  if (!power->priv->can_suspend )
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_NO_HARDWARE_SUPPORT,
                                           _("Suspend not supported"));
    return TRUE;
  }

  espm_power_sleep (power, "Suspend", FALSE);

  espm_power_management_complete_suspend (user_data, invocation);

  return TRUE;
}

static gboolean
espm_power_dbus_can_reboot (EspmPower * power,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
  gboolean can_reboot;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-reboot", &can_reboot,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-reboot", &can_reboot,
                  NULL);
  }

  espm_power_management_complete_can_reboot (user_data,
                                             invocation,
                                             can_reboot);

  return TRUE;
}

static gboolean
espm_power_dbus_can_shutdown (EspmPower * power,
                              GDBusMethodInvocation *invocation,
                              gpointer user_data)
{
  gboolean can_shutdown;

  if ( LOGIND_RUNNING () )
  {
    g_object_get (G_OBJECT (power->priv->systemd),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }
  else
  {
    g_object_get (G_OBJECT (power->priv->console),
                  "can-shutdown", &can_shutdown,
                  NULL);
  }

  espm_power_management_complete_can_shutdown (user_data,
                                               invocation,
                                               can_shutdown);

  return TRUE;
}

static gboolean
espm_power_dbus_can_hibernate (EspmPower * power,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data)
{
  espm_power_management_complete_can_hibernate (user_data,
                                                invocation,
                                                power->priv->can_hibernate);
  return TRUE;
}

static gboolean
espm_power_dbus_can_suspend (EspmPower * power,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
  espm_power_management_complete_can_suspend (user_data,
                                              invocation,
                                              power->priv->can_suspend);

  return TRUE;
}

static gboolean
espm_power_dbus_get_on_battery (EspmPower * power,
                                GDBusMethodInvocation *invocation,
                                gpointer user_data)
{
  espm_power_management_complete_get_on_battery (user_data,
                                                 invocation,
                                                 power->priv->on_battery);

  return TRUE;
}

static gboolean
espm_power_dbus_get_low_battery (EspmPower * power,
                                 GDBusMethodInvocation *invocation,
                                 gpointer user_data)
{
  espm_power_management_complete_get_low_battery (user_data,
                                                  invocation,
                                                  power->priv->on_low_battery);

  return TRUE;
}
