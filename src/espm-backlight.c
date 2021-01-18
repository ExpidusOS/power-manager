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
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#include <libexpidus1util/libexpidus1util.h>

#include "espm-backlight.h"
#include "egg-idletime.h"
#include "espm-notify.h"
#include "espm-esconf.h"
#include "espm-power.h"
#include "espm-config.h"
#include "espm-button.h"
#include "espm-brightness.h"
#include "espm-debug.h"
#include "espm-icons.h"

static void espm_backlight_finalize     (GObject *object);

static void espm_backlight_get_property (GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);

static void espm_backlight_set_property (GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec);

#define ALARM_DISABLED 9

struct EspmBacklightPrivate
{
  EspmBrightness *brightness;
  EspmPower      *power;
  EggIdletime    *idle;
  EspmEsconf     *conf;
  EspmButton     *button;
  EspmNotify     *notify;

  NotifyNotification *n;

  gboolean      has_hw;
  gboolean      on_battery;

  gint32          last_level;
  gint32      max_level;

  guint           brightness_step_count;
  gboolean        brightness_exponential;

  gint            brightness_switch;
  gint            brightness_switch_save;
  gboolean        brightness_switch_initialized;

  gboolean        dimmed;
  gboolean      block;
};

enum
{
  PROP_0,
  PROP_BRIGHTNESS_SWITCH,
  PROP_BRIGHTNESS_SWITCH_SAVE,
  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (EspmBacklight, espm_backlight, G_TYPE_OBJECT)


static void
espm_backlight_dim_brightness (EspmBacklight *backlight)
{
  gboolean ret;

  if (espm_power_is_in_presentation_mode (backlight->priv->power) == FALSE )
  {
    gint32 dim_level;

    g_object_get (G_OBJECT (backlight->priv->conf),
                  backlight->priv->on_battery ? BRIGHTNESS_LEVEL_ON_BATTERY : BRIGHTNESS_LEVEL_ON_AC, &dim_level,
                  NULL);

    ret = espm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);

    if ( !ret )
    {
      g_warning ("Unable to get current brightness level");
      return;
    }

    dim_level = dim_level * backlight->priv->max_level / 100;

    /**
     * Only reduce if the current level is brighter than
     * the configured dim_level
     **/
    if (backlight->priv->last_level > dim_level)
    {
      ESPM_DEBUG ("Current brightness level before dimming : %d, new %d", backlight->priv->last_level, dim_level);
      backlight->priv->dimmed = espm_brightness_set_level (backlight->priv->brightness, dim_level);
    }
  }
}

static gboolean
espm_backlight_destroy_popup (gpointer data)
{
  EspmBacklight *backlight;

  backlight = ESPM_BACKLIGHT (data);

  if ( backlight->priv->n )
  {
    g_object_unref (backlight->priv->n);
    backlight->priv->n = NULL;
  }

  return FALSE;
}

static void
espm_backlight_show_notification (EspmBacklight *backlight, gfloat value)
{
  gchar *summary;

  /* create the notification on demand */
  if ( backlight->priv->n == NULL )
  {
    /* generate a human-readable summary for the notification */
    summary = g_strdup_printf (_("Brightness: %.0f percent"), value);
    backlight->priv->n = espm_notify_new_notification (backlight->priv->notify,
                                                       _("Power Manager"),
                                                       summary,
                                                       ESPM_DISPLAY_BRIGHTNESS_ICON,
                                                       ESPM_NOTIFY_NORMAL);
    g_free (summary);
  }

  /* add the brightness value to the notification */
  notify_notification_set_hint (backlight->priv->n, "value", g_variant_new_int32 (value));

  /* show the notification */
  notify_notification_show (backlight->priv->n, NULL);
}

static void
espm_backlight_show (EspmBacklight *backlight, gint level)
{
  gfloat value;

  ESPM_DEBUG ("Level %u", level);

  value = (gfloat) 100 * level / backlight->priv->max_level;
  espm_backlight_show_notification (backlight, value);
}


static void
espm_backlight_alarm_timeout_cb (EggIdletime *idle, guint id, EspmBacklight *backlight)
{
  backlight->priv->block = FALSE;

  if ( id == TIMEOUT_BRIGHTNESS_ON_AC && !backlight->priv->on_battery)
    espm_backlight_dim_brightness (backlight);
  else if ( id == TIMEOUT_BRIGHTNESS_ON_BATTERY && backlight->priv->on_battery)
    espm_backlight_dim_brightness (backlight);
}

static void
espm_backlight_reset_cb (EggIdletime *idle, EspmBacklight *backlight)
{
  if ( backlight->priv->dimmed)
  {
    if ( !backlight->priv->block)
    {
      ESPM_DEBUG ("Alarm reset, setting level to %d", backlight->priv->last_level);
      espm_brightness_set_level (backlight->priv->brightness, backlight->priv->last_level);
    }
    backlight->priv->dimmed = FALSE;
  }
}

static void
espm_backlight_button_pressed_cb (EspmButton *button, EspmButtonKey type, EspmBacklight *backlight)
{
  gint32   level;
  gboolean ret = TRUE;
  gboolean handle_brightness_keys, show_popup;
  guint    brightness_step_count;
  gboolean brightness_exponential;

  g_object_get (G_OBJECT (backlight->priv->conf),
                HANDLE_BRIGHTNESS_KEYS, &handle_brightness_keys,
                SHOW_BRIGHTNESS_POPUP, &show_popup,
                BRIGHTNESS_STEP_COUNT, &brightness_step_count,
                BRIGHTNESS_EXPONENTIAL, &brightness_exponential,
                NULL);

  if ( type != BUTTON_MON_BRIGHTNESS_UP && type != BUTTON_MON_BRIGHTNESS_DOWN )
    return; /* sanity check, can this ever happen? */

  backlight->priv->block = TRUE;
  if ( !handle_brightness_keys )
    ret = espm_brightness_get_level (backlight->priv->brightness, &level);
  else
  {
    espm_brightness_set_step_count(backlight->priv->brightness,
                                   brightness_step_count,
                                   brightness_exponential);
    if ( type == BUTTON_MON_BRIGHTNESS_UP )
      ret = espm_brightness_up (backlight->priv->brightness, &level);
    else
      ret = espm_brightness_down (backlight->priv->brightness, &level);
  }
  if ( ret && show_popup)
    espm_backlight_show (backlight, level);
}

static void
espm_backlight_brightness_on_ac_settings_changed (EspmBacklight *backlight)
{
  guint timeout_on_ac;

  g_object_get (G_OBJECT (backlight->priv->conf),
                BRIGHTNESS_ON_AC, &timeout_on_ac,
                NULL);

  ESPM_DEBUG ("Alarm on ac timeout changed %u", timeout_on_ac);

  if ( timeout_on_ac == ALARM_DISABLED )
  {
    egg_idletime_alarm_remove (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_AC );
  }
  else
  {
    egg_idletime_alarm_set (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_AC, timeout_on_ac * 1000);
  }
}

static void
espm_backlight_brightness_on_battery_settings_changed (EspmBacklight *backlight)
{
  guint timeout_on_battery ;

  g_object_get (G_OBJECT (backlight->priv->conf),
                BRIGHTNESS_ON_BATTERY, &timeout_on_battery,
                NULL);

  ESPM_DEBUG ("Alarm on battery timeout changed %u", timeout_on_battery);

  if ( timeout_on_battery == ALARM_DISABLED )
  {
    egg_idletime_alarm_remove (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_BATTERY );
  }
  else
  {
    egg_idletime_alarm_set (backlight->priv->idle, TIMEOUT_BRIGHTNESS_ON_BATTERY, timeout_on_battery * 1000);
  }
}


static void
espm_backlight_set_timeouts (EspmBacklight *backlight)
{
  espm_backlight_brightness_on_ac_settings_changed (backlight);
  espm_backlight_brightness_on_battery_settings_changed (backlight);
}

static void
espm_backlight_on_battery_changed_cb (EspmPower *power, gboolean on_battery, EspmBacklight *backlight)
{
  backlight->priv->on_battery = on_battery;
}

static void
espm_backlight_class_init (EspmBacklightClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = espm_backlight_get_property;
  object_class->set_property = espm_backlight_set_property;
  object_class->finalize = espm_backlight_finalize;

  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SWITCH,
                                   g_param_spec_int (BRIGHTNESS_SWITCH,
                                                     NULL, NULL,
                                                     -1,
                                                     1,
                                                     -1,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SWITCH_SAVE,
                                   g_param_spec_int (BRIGHTNESS_SWITCH_SAVE,
                                                     NULL, NULL,
                                                     -1,
                                                     1,
                                                     -1,
                                                     G_PARAM_READWRITE));
}

static void
espm_backlight_init (EspmBacklight *backlight)
{
  backlight->priv = espm_backlight_get_instance_private (backlight);

  backlight->priv->brightness = espm_brightness_new ();
  backlight->priv->has_hw     = espm_brightness_setup (backlight->priv->brightness);

  backlight->priv->notify = NULL;
  backlight->priv->idle   = NULL;
  backlight->priv->conf   = NULL;
  backlight->priv->button = NULL;
  backlight->priv->power    = NULL;
  backlight->priv->dimmed = FALSE;
  backlight->priv->block = FALSE;
  backlight->priv->brightness_step_count = 10;
  backlight->priv->brightness_exponential = FALSE;
  backlight->priv->brightness_switch_initialized = FALSE;

  if ( !backlight->priv->has_hw )
  {
    g_object_unref (backlight->priv->brightness);
    backlight->priv->brightness = NULL;
  }
  else
  {
    gboolean ret, handle_keys;

    backlight->priv->idle   = egg_idletime_new ();
    backlight->priv->conf   = espm_esconf_new ();
    backlight->priv->button = espm_button_new ();
    backlight->priv->power    = espm_power_get ();
    backlight->priv->notify = espm_notify_new ();
    backlight->priv->max_level = espm_brightness_get_max_level (backlight->priv->brightness);
    backlight->priv->brightness_switch = -1;

    esconf_g_property_bind (espm_esconf_get_channel (backlight->priv->conf),
                            ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH, G_TYPE_INT,
                            G_OBJECT (backlight), BRIGHTNESS_SWITCH);

    ret = espm_brightness_get_switch (backlight->priv->brightness,
                      &backlight->priv->brightness_switch);

    if (ret)
      g_object_set (G_OBJECT (backlight),
                    BRIGHTNESS_SWITCH,
                    backlight->priv->brightness_switch,
                    NULL);
    backlight->priv->brightness_switch_initialized = TRUE;

    /*
     * If power manager has crashed last time, the brightness switch
     * saved value will be set to the original value. In that case, we
     * will use this saved value instead of the one found at the
     * current startup so the setting is restored properly.
     */
    backlight->priv->brightness_switch_save =
      esconf_channel_get_int (espm_esconf_get_channel(backlight->priv->conf),
                  ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_SAVE,
                  -1);

    if (backlight->priv->brightness_switch_save == -1)
    {
      if (!esconf_channel_set_int (espm_esconf_get_channel(backlight->priv->conf),
                     ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_SAVE,
                     backlight->priv->brightness_switch))
      g_critical ("Cannot set value for property %s\n", BRIGHTNESS_SWITCH_SAVE);

      backlight->priv->brightness_switch_save = backlight->priv->brightness_switch;
    }
    else
    {
      g_warning ("It seems the kernel brightness switch handling value was "
                 "not restored properly on exit last time, expidus1-power-manager "
                 "will try to restore it this time.");
    }

      /* check whether to change the brightness switch */
    handle_keys = esconf_channel_get_bool (espm_esconf_get_channel(backlight->priv->conf),
                         ESPM_PROPERTIES_PREFIX HANDLE_BRIGHTNESS_KEYS,
                         TRUE);
    backlight->priv->brightness_switch = handle_keys ? 0 : 1;
    g_object_set (G_OBJECT (backlight),
            BRIGHTNESS_SWITCH,
            backlight->priv->brightness_switch,
            NULL);

    g_signal_connect (backlight->priv->idle, "alarm-expired",
                      G_CALLBACK (espm_backlight_alarm_timeout_cb), backlight);
    g_signal_connect (backlight->priv->idle, "reset",
                      G_CALLBACK(espm_backlight_reset_cb), backlight);
    g_signal_connect (backlight->priv->button, "button-pressed",
                      G_CALLBACK (espm_backlight_button_pressed_cb), backlight);
    g_signal_connect_swapped (backlight->priv->conf, "notify::" BRIGHTNESS_ON_AC,
                              G_CALLBACK (espm_backlight_brightness_on_ac_settings_changed), backlight);
    g_signal_connect_swapped (backlight->priv->conf, "notify::" BRIGHTNESS_ON_BATTERY,
                              G_CALLBACK (espm_backlight_brightness_on_battery_settings_changed), backlight);
    g_signal_connect (backlight->priv->power, "on-battery-changed",
                      G_CALLBACK (espm_backlight_on_battery_changed_cb), backlight);

    g_object_get (G_OBJECT (backlight->priv->power),
                  "on-battery", &backlight->priv->on_battery,
                  NULL);
    espm_brightness_get_level (backlight->priv->brightness, &backlight->priv->last_level);
    espm_backlight_set_timeouts (backlight);

    /* setup step count */
    backlight->priv->brightness_step_count =
        esconf_channel_get_int (espm_esconf_get_channel(backlight->priv->conf),
                                ESPM_PROPERTIES_PREFIX BRIGHTNESS_STEP_COUNT,
                                10);
    backlight->priv->brightness_exponential =
        esconf_channel_get_bool (espm_esconf_get_channel(backlight->priv->conf),
                                 ESPM_PROPERTIES_PREFIX BRIGHTNESS_EXPONENTIAL,
                                 FALSE);
  }
}

static void
espm_backlight_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  EspmBacklight *backlight = ESPM_BACKLIGHT (object);

  switch (prop_id)
  {
    case PROP_BRIGHTNESS_SWITCH:
      g_value_set_int (value, backlight->priv->brightness_switch);
      break;
    case PROP_BRIGHTNESS_SWITCH_SAVE:
      g_value_set_int (value, backlight->priv->brightness_switch_save);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_backlight_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
  EspmBacklight *backlight = ESPM_BACKLIGHT (object);
  gboolean ret;

  switch (prop_id)
  {
    case PROP_BRIGHTNESS_SWITCH:
      backlight->priv->brightness_switch = g_value_get_int (value);
      if (!backlight->priv->brightness_switch_initialized)
          break;
      ret = espm_brightness_set_switch (backlight->priv->brightness,
                                        backlight->priv->brightness_switch);
      if (!ret)
          g_warning ("Unable to set the kernel brightness switch parameter to %d.",
                     backlight->priv->brightness_switch);
      else
          g_message ("Set kernel brightness switch to %d",
                     backlight->priv->brightness_switch);

      break;
    case PROP_BRIGHTNESS_SWITCH_SAVE:
      backlight->priv->brightness_switch_save = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_backlight_finalize (GObject *object)
{
  EspmBacklight *backlight;

  backlight = ESPM_BACKLIGHT (object);

  espm_backlight_destroy_popup (backlight);

  if ( backlight->priv->idle )
    g_object_unref (backlight->priv->idle);

  if ( backlight->priv->conf )
  {
  /* restore video module brightness switch setting */
  if ( backlight->priv->brightness_switch_save != -1 )
  {
    gboolean ret =
        espm_brightness_set_switch (backlight->priv->brightness,
                                    backlight->priv->brightness_switch_save);
    /* unset the esconf saved value after the restore */
    if (!esconf_channel_set_int (espm_esconf_get_channel(backlight->priv->conf),
                                 ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_SAVE, -1))
    g_critical ("Cannot set value for property %s\n", BRIGHTNESS_SWITCH_SAVE);

    if (ret)
    {
      backlight->priv->brightness_switch = backlight->priv->brightness_switch_save;
      g_message ("Restored brightness switch value to: %d", backlight->priv->brightness_switch);
    }
    else
      g_warning ("Unable to restore the kernel brightness switch parameter to its original value, "
                 "still resetting the saved value.");
    }
    g_object_unref (backlight->priv->conf);
  }

  if ( backlight->priv->brightness )
    g_object_unref (backlight->priv->brightness);

  if ( backlight->priv->button )
    g_object_unref (backlight->priv->button);

  if ( backlight->priv->power )
    g_object_unref (backlight->priv->power);

  if ( backlight->priv->notify)
    g_object_unref (backlight->priv->notify);

  G_OBJECT_CLASS (espm_backlight_parent_class)->finalize (object);
}

EspmBacklight *
espm_backlight_new (void)
{
  EspmBacklight *backlight = NULL;
  backlight = g_object_new (ESPM_TYPE_BACKLIGHT, NULL);
  return backlight;
}

gboolean espm_backlight_has_hw (EspmBacklight *backlight)
{
  return backlight->priv->has_hw;
}
