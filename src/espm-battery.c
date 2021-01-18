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
#include <upower.h>

#include <libexpidus1util/libexpidus1util.h>

#include "espm-battery.h"
#include "espm-dbus.h"
#include "espm-icons.h"
#include "espm-esconf.h"
#include "espm-notify.h"
#include "espm-config.h"
#include "espm-button.h"
#include "espm-enum-glib.h"
#include "espm-enum-types.h"
#include "espm-debug.h"
#include "espm-power-common.h"
#include "espm-common.h"

static void espm_battery_finalize   (GObject *object);

struct EspmBatteryPrivate
{
  EspmEsconf             *conf;
  EspmNotify             *notify;
  EspmButton             *button;
  UpDevice               *device;
  UpClient               *client;

  EspmBatteryCharge       charge;
  UpDeviceState           state;
  UpDeviceKind            type;
  gboolean                ac_online;
  gboolean                present;
  guint                   percentage;
  gint64                  time_to_full;
  gint64                  time_to_empty;

  const gchar            *battery_name;

  gulong                  sig;
  gulong                  sig_bt;
  gulong                  sig_up;

  guint                   notify_idle;
};

enum
{
  PROP_0,
  PROP_AC_ONLINE,
  PROP_CHARGE_STATUS,
  PROP_DEVICE_TYPE
};

enum
{
  BATTERY_CHARGE_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EspmBattery, espm_battery, GTK_TYPE_WIDGET)


static gchar *
espm_battery_get_message_from_battery_state (EspmBattery *battery)
{
  gchar *msg  = NULL;


  if (battery->priv->type == UP_DEVICE_KIND_BATTERY || battery->priv->type == UP_DEVICE_KIND_UPS)
  {
    switch (battery->priv->state)
    {
      case UP_DEVICE_STATE_FULLY_CHARGED:
        msg = g_strdup_printf (_("Your %s is fully charged"), battery->priv->battery_name);
        break;
      case UP_DEVICE_STATE_CHARGING:
        msg = g_strdup_printf (_("Your %s is charging"), battery->priv->battery_name);

        if ( battery->priv->time_to_full != 0 )
        {
          gchar *tmp, *est_time_str;
          tmp = g_strdup (msg);
          g_free (msg);

          est_time_str = espm_battery_get_time_string (battery->priv->time_to_full);

          msg = g_strdup_printf (_("%s (%i%%)\n%s until fully charged"), tmp, battery->priv->percentage, est_time_str);
          g_free (est_time_str);
          g_free (tmp);
        }

        break;
      case UP_DEVICE_STATE_DISCHARGING:
        if (battery->priv->ac_online)
            msg =  g_strdup_printf (_("Your %s is discharging"), battery->priv->battery_name);
        else
            msg =  g_strdup_printf (_("System is running on %s power"), battery->priv->battery_name);

        if ( battery->priv->time_to_empty != 0 )
        {
            gchar *tmp, *est_time_str;
            tmp = g_strdup (msg);
            g_free (msg);

            est_time_str = espm_battery_get_time_string (battery->priv->time_to_empty);

            msg = g_strdup_printf (_("%s (%i%%)\nEstimated time left is %s"), tmp, battery->priv->percentage, est_time_str);
            g_free (tmp);
            g_free (est_time_str);
        }
        break;
      case UP_DEVICE_STATE_EMPTY:
        msg = g_strdup_printf (_("Your %s is empty"), battery->priv->battery_name);
        break;
      default:
        break;
    }

  }
  else if (battery->priv->type >= UP_DEVICE_KIND_MONITOR)
  {
    switch (battery->priv->state)
    {
      case UP_DEVICE_STATE_FULLY_CHARGED:
        msg = g_strdup_printf (_("Your %s is fully charged"), battery->priv->battery_name);
        break;
      case UP_DEVICE_STATE_CHARGING:
        msg = g_strdup_printf (_("Your %s is charging"), battery->priv->battery_name);
        break;
      case UP_DEVICE_STATE_DISCHARGING:
        msg =  g_strdup_printf (_("Your %s is discharging"), battery->priv->battery_name);
        break;
      case UP_DEVICE_STATE_EMPTY:
        msg = g_strdup_printf (_("Your %s is empty"), battery->priv->battery_name);
        break;
      default:
        break;
    }
  }

  return msg;
}

static void
espm_battery_notify (EspmBattery *battery)
{
  gchar *message = NULL;

  message = espm_battery_get_message_from_battery_state (battery);

  if ( !message )
    return;

  espm_notify_show_notification (battery->priv->notify,
                                 _("Power Manager"),
                                 message,
                                 espm_battery_get_icon_name (battery),
                                 ESPM_NOTIFY_NORMAL);

  g_free (message);
}

static gboolean
espm_battery_notify_idle (gpointer data)
{
  EspmBattery *battery = ESPM_BATTERY (data);

  espm_battery_notify (battery);
  battery->priv->notify_idle = 0;

  return FALSE;
}

static void
espm_battery_notify_state (EspmBattery *battery)
{
  gboolean notify;
  static gboolean starting_up = TRUE;

  if ( battery->priv->type == UP_DEVICE_KIND_BATTERY ||
       battery->priv->type == UP_DEVICE_KIND_UPS )
  {
    if ( starting_up )
    {
      starting_up = FALSE;
      return;
    }

    g_object_get (G_OBJECT (battery->priv->conf),
                  GENERAL_NOTIFICATION_CFG, &notify,
                  NULL);

    if ( notify )
    {
      if (battery->priv->notify_idle == 0)
        battery->priv->notify_idle = g_idle_add (espm_battery_notify_idle, battery);
    }
  }
}

static void
espm_battery_check_charge (EspmBattery *battery)
{
  EspmBatteryCharge charge;
  guint critical_level, low_level;

  g_object_get (G_OBJECT (battery->priv->conf),
                CRITICAL_POWER_LEVEL, &critical_level,
                NULL);

  low_level = critical_level + 10;

  if ( battery->priv->percentage > low_level )
    charge = ESPM_BATTERY_CHARGE_OK;
  else if ( battery->priv->percentage <= low_level && battery->priv->percentage > critical_level )
    charge = ESPM_BATTERY_CHARGE_LOW;
  else if ( battery->priv->percentage <= critical_level )
    charge = ESPM_BATTERY_CHARGE_CRITICAL;
  else
    charge = ESPM_BATTERY_CHARGE_UNKNOWN;

  if ( charge != battery->priv->charge)
  {
    battery->priv->charge = charge;
    /*
     * only emit signal when when battery charge changes from ok->low->critical
     * and not the other way round.
     */
    if ( battery->priv->charge != ESPM_BATTERY_CHARGE_CRITICAL || charge != ESPM_BATTERY_CHARGE_LOW )
      g_signal_emit (G_OBJECT (battery), signals [BATTERY_CHARGE_CHANGED], 0);
  }
}

static void
espm_battery_refresh (EspmBattery *battery, UpDevice *device)
{
  gboolean present;
  guint state;
  gdouble percentage;
  guint64 to_empty, to_full;

  g_object_get (device,
                "is-present", &present,
                "percentage", &percentage,
                "state", &state,
                "time-to-empty", &to_empty,
                "time-to-full", &to_full,
                NULL);

  battery->priv->present = present;
  if ( state != battery->priv->state )
  {
    battery->priv->state = state;
    espm_battery_notify_state (battery);
  }
  battery->priv->percentage = (guint) percentage;

  espm_battery_check_charge (battery);

  if ( battery->priv->type == UP_DEVICE_KIND_BATTERY ||
       battery->priv->type == UP_DEVICE_KIND_UPS )
  {
    battery->priv->time_to_empty = to_empty;
    battery->priv->time_to_full  = to_empty;
  }
}

static void
espm_battery_changed_cb (UpDevice *device,
                         GParamSpec *pspec,
                         EspmBattery *battery)
{
  espm_battery_refresh (battery, device);
}

static void
espm_battery_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EspmBattery *battery;

  battery = ESPM_BATTERY (object);

  switch (prop_id)
  {
    case PROP_AC_ONLINE:
      g_value_set_boolean (value, battery->priv->ac_online);
      break;
    case PROP_DEVICE_TYPE:
      g_value_set_uint (value, battery->priv->type);
      break;
    case PROP_CHARGE_STATUS:
      g_value_set_enum (value, battery->priv->charge);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_battery_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EspmBattery *battery;

  battery = ESPM_BATTERY (object);

  switch (prop_id)
  {
    case PROP_AC_ONLINE:
      battery->priv->ac_online = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
espm_battery_class_init (EspmBatteryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = espm_battery_finalize;
  object_class->get_property = espm_battery_get_property;
  object_class->set_property = espm_battery_set_property;

  signals [BATTERY_CHARGE_CHANGED] =
      g_signal_new ("battery-charge-changed",
                    ESPM_TYPE_BATTERY,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET(EspmBatteryClass, battery_charge_changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0, G_TYPE_NONE);

  g_object_class_install_property (object_class,
                                   PROP_AC_ONLINE,
                                   g_param_spec_boolean ("ac-online",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DEVICE_TYPE,
                                   g_param_spec_uint ("device-type",
                                                      NULL, NULL,
                                                      UP_DEVICE_KIND_UNKNOWN,
                                                      UP_DEVICE_KIND_LAST,
                                                      UP_DEVICE_KIND_UNKNOWN,
                                                      G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CHARGE_STATUS,
                                   g_param_spec_enum ("charge-status",
                                                      NULL, NULL,
                                                      ESPM_TYPE_BATTERY_CHARGE,
                                                      ESPM_BATTERY_CHARGE_UNKNOWN,
                                                      G_PARAM_READABLE));
}

static void
espm_battery_init (EspmBattery *battery)
{
  battery->priv = espm_battery_get_instance_private (battery);

  battery->priv->conf          = espm_esconf_new ();
  battery->priv->notify        = espm_notify_new ();
  battery->priv->device        = NULL;
  battery->priv->client        = NULL;
  battery->priv->state         = UP_DEVICE_STATE_UNKNOWN;
  battery->priv->type          = UP_DEVICE_KIND_UNKNOWN;
  battery->priv->charge        = ESPM_BATTERY_CHARGE_UNKNOWN;
  battery->priv->time_to_full  = 0;
  battery->priv->time_to_empty = 0;
  battery->priv->button        = espm_button_new ();
  battery->priv->ac_online     = TRUE;
}

static void
espm_battery_finalize (GObject *object)
{
  EspmBattery *battery;

  battery = ESPM_BATTERY (object);

  if (battery->priv->notify_idle != 0)
    g_source_remove (battery->priv->notify_idle);

  if ( g_signal_handler_is_connected (battery->priv->device, battery->priv->sig_up ) )
    g_signal_handler_disconnect (G_OBJECT (battery->priv->device), battery->priv->sig_up);

  if ( g_signal_handler_is_connected (battery->priv->conf, battery->priv->sig ) )
    g_signal_handler_disconnect (G_OBJECT (battery->priv->conf), battery->priv->sig);

  if ( g_signal_handler_is_connected (battery->priv->button, battery->priv->sig_bt ) )
    g_signal_handler_disconnect (G_OBJECT (battery->priv->button), battery->priv->sig_bt);

  g_object_unref (battery->priv->device);
  g_object_unref (battery->priv->conf);
  g_object_unref (battery->priv->notify);
  g_object_unref (battery->priv->button);

  G_OBJECT_CLASS (espm_battery_parent_class)->finalize (object);
}

GtkWidget *
espm_battery_new (void)
{
  EspmBattery *battery = NULL;

  battery = g_object_new (ESPM_TYPE_BATTERY, NULL);

  return GTK_WIDGET (battery);
}

void
espm_battery_monitor_device (EspmBattery *battery,
                             const char *object_path,
                             UpDeviceKind device_type)
{
  UpDevice *device;
  battery->priv->type = device_type;
  battery->priv->client = up_client_new();
  battery->priv->battery_name = espm_power_translate_device_type (device_type);

  device = up_device_new();
  up_device_set_object_path_sync (device, object_path, NULL, NULL);
  battery->priv->device = device;
  battery->priv->sig_up = g_signal_connect (battery->priv->device, "notify", G_CALLBACK (espm_battery_changed_cb), battery);

  g_object_set (G_OBJECT (battery),
                "has-tooltip", TRUE,
                NULL);

  espm_battery_refresh (battery, device);
}

UpDeviceKind
espm_battery_get_device_type (EspmBattery *battery)
{
  g_return_val_if_fail (ESPM_IS_BATTERY (battery), UP_DEVICE_KIND_UNKNOWN );

  return battery->priv->type;
}

EspmBatteryCharge
espm_battery_get_charge (EspmBattery *battery)
{
  g_return_val_if_fail (ESPM_IS_BATTERY (battery), ESPM_BATTERY_CHARGE_UNKNOWN);

  return battery->priv->charge;
}

const gchar *
espm_battery_get_battery_name (EspmBattery *battery)
{
  g_return_val_if_fail (ESPM_IS_BATTERY (battery), NULL);

  return battery->priv->battery_name;
}

gchar *
espm_battery_get_time_left (EspmBattery *battery)
{
  g_return_val_if_fail (ESPM_IS_BATTERY (battery), NULL);

  return espm_battery_get_time_string (battery->priv->time_to_empty);
}

const gchar*
espm_battery_get_icon_name (EspmBattery *battery)
{
  g_return_val_if_fail (ESPM_IS_BATTERY (battery), NULL);

  return get_device_icon_name (battery->priv->client, battery->priv->device, TRUE);
}
