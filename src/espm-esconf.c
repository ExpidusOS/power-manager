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

#include <glib.h>
#include <libexpidus1util/libexpidus1util.h>

#include "espm-esconf.h"
#include "espm-config.h"
#include "espm-enum-glib.h"
#include "espm-enum.h"
#include "espm-enum-types.h"
#include "espm-debug.h"

static void espm_esconf_finalize   (GObject *object);

struct EspmEsconfPrivate
{
  EsconfChannel   *channel;
  EsconfChannel   *session_channel;
  GValue          *values;
};

enum
{
  PROP_0,
  PROP_GENERAL_NOTIFICATION,
  PROP_LOCK_SCREEN_ON_SLEEP,
  PROP_CRITICAL_LEVEL,
  PROP_SHOW_BRIGHTNESS_POPUP,
  PROP_HANDLE_BRIGHTNESS_KEYS,
  PROP_BRIGHTNESS_STEP_COUNT,
  PROP_BRIGHTNESS_EXPONENTIAL,
  PROP_TRAY_ICON,
  PROP_CRITICAL_BATTERY_ACTION,
  PROP_POWER_BUTTON,
  PROP_HIBERNATE_BUTTON,
  PROP_SLEEP_BUTTON,
  PROP_BATTERY_BUTTON,
  PROP_LID_ACTION_ON_AC,
  PROP_LID_ACTION_ON_BATTERY,
  PROP_BRIGHTNESS_LEVEL_ON_AC,
  PROP_BRIGHTNESS_LEVEL_ON_BATTERY,
  PROP_BRIGHTNESS_SLIDER_MIN_LEVEL,

  PROP_ENABLE_DPMS,
  PROP_DPMS_SLEEP_ON_AC,
  PROP_DPMS_OFF_ON_AC,
  PROP_DPMS_SLEEP_ON_BATTERY,
  PROP_DPMS_OFF_ON_BATTERY,
  PROP_DPMS_SLEEP_MODE,

  PROP_IDLE_ON_AC,
  PROP_IDLE_ON_BATTERY,
  PROP_IDLE_SLEEP_MODE_ON_AC,
  PROP_IDLE_SLEEP_MODE_ON_BATTERY,
  PROP_DIM_ON_AC_TIMEOUT,
  PROP_DIM_ON_BATTERY_TIMEOUT,
#ifdef WITH_NETWORK_MANAGER
  PROP_NETWORK_MANAGER_SLEEP,
#endif
  PROP_LOGIND_HANDLE_POWER_KEY,
  PROP_LOGIND_HANDLE_SUSPEND_KEY,
  PROP_LOGIND_HANDLE_HIBERNATE_KEY,
  PROP_LOGIND_HANDLE_LID_SWITCH,
  PROP_HEARTBEAT_COMMAND,
  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (EspmEsconf, espm_esconf, G_TYPE_OBJECT)

static void
espm_esconf_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  EspmEsconf *conf;
  GValue *dst;

  conf = ESPM_ESCONF (object);

  dst = conf->priv->values + prop_id;

  if ( !G_IS_VALUE (dst) )
  {
    g_value_init (dst, pspec->value_type);
    g_param_value_set_default (pspec, dst);
  }

  if ( g_param_values_cmp (pspec, value, dst) != 0)
  {
    g_value_copy (value, dst);
    g_object_notify (object, pspec->name);
  }
}

static void
espm_esconf_get_property (GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  EspmEsconf *conf;
  GValue *src;

  conf = ESPM_ESCONF (object);

  src = conf->priv->values + prop_id;

  if ( G_VALUE_HOLDS (src, pspec->value_type) )
    g_value_copy (src, value);
  else
    g_param_value_set_default (pspec, value);
}

static void
espm_esconf_load (EspmEsconf *conf, gboolean channel_valid)
{
  GParamSpec **specs;
  GValue value = { 0, };
  guint nspecs;
  guint i;

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (conf), &nspecs);

  for ( i = 0; i < nspecs; i++)
  {
    gchar *prop_name;
    prop_name = g_strdup_printf ("%s%s", ESPM_PROPERTIES_PREFIX, specs[i]->name);
    g_value_init (&value, specs[i]->value_type);

    if (channel_valid)
    {
      if ( !esconf_channel_get_property (conf->priv->channel, prop_name, &value) )
      {
        ESPM_DEBUG ("Using default configuration for %s", specs[i]->name);
        g_param_value_set_default (specs[i], &value);
      }
    }
    else
    {
      ESPM_DEBUG ("Using default configuration for %s", specs[i]->name);
      g_param_value_set_default (specs[i], &value);
    }
    g_free (prop_name);
    g_object_set_property (G_OBJECT (conf), specs[i]->name, &value);
    g_value_unset (&value);
  }
  g_free (specs);
}

static void
espm_esconf_property_changed_cb (EsconfChannel *channel, gchar *property,
         GValue *value, EspmEsconf *conf)
{
  /*FIXME: Set default for this key*/
  if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    return;

  if ( !g_str_has_prefix (property, ESPM_PROPERTIES_PREFIX) || strlen (property) <= strlen (ESPM_PROPERTIES_PREFIX) )
    return;

  /* We handle presentation mode and blank-times in espm-power directly */
  if ( g_strcmp0 (property, ESPM_PROPERTIES_PREFIX PRESENTATION_MODE) == 0 ||
       g_strcmp0 (property, ESPM_PROPERTIES_PREFIX ON_AC_BLANK) == 0 ||
       g_strcmp0 (property, ESPM_PROPERTIES_PREFIX ON_BATTERY_BLANK) == 0)
    return;

  /* We handle brightness switch in espm-backlight directly */
  if ( g_strcmp0 (property, ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH) == 0 ||
       g_strcmp0 (property, ESPM_PROPERTIES_PREFIX BRIGHTNESS_SWITCH_SAVE) == 0 )
    return;

  ESPM_DEBUG ("Property modified: %s\n", property);

  g_object_set_property (G_OBJECT (conf), property + strlen (ESPM_PROPERTIES_PREFIX), value);
}

static void
espm_xfsession_property_changed_cb (EsconfChannel *channel, gchar *property,
         GValue *value, EspmEsconf *conf)
{
  /*FIXME: Set default for this key*/
  if ( G_VALUE_TYPE(value) == G_TYPE_INVALID )
    return;

  ESPM_DEBUG ("property %s\n", property);

  if ( g_strcmp0 (property, "/shutdown/LockScreen") != 0)
    return;

  /* sanity check */
  if ( !G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) )
    return;

  ESPM_DEBUG ("Property modified: %s\n", property);

  /* update esconf which will update espm and keep things in sync */
  esconf_channel_set_bool (conf->priv->channel,
                           ESPM_PROPERTIES_PREFIX LOCK_SCREEN_ON_SLEEP,
                           g_value_get_boolean(value));
}

static void
espm_esconf_class_init (EspmEsconfClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = espm_esconf_set_property;
  object_class->get_property = espm_esconf_get_property;

  object_class->finalize = espm_esconf_finalize;

  /**
   * EspmEsconf::general-notification
   **/
  g_object_class_install_property (object_class,
                                   PROP_GENERAL_NOTIFICATION,
                                   g_param_spec_boolean (GENERAL_NOTIFICATION_CFG,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  /**
   * EspmEsconf::lock-screen-suspend-hibernate
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOCK_SCREEN_ON_SLEEP,
                                   g_param_spec_boolean (LOCK_SCREEN_ON_SLEEP,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::critical-power-level
   **/
  g_object_class_install_property (object_class,
                                   PROP_CRITICAL_LEVEL,
                                   g_param_spec_uint (CRITICAL_POWER_LEVEL,
                                                      NULL, NULL,
                                                      1,
                                                      20,
                                                      5,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::show-brightness-popup
   **/
  g_object_class_install_property (object_class,
                                   PROP_SHOW_BRIGHTNESS_POPUP,
                                   g_param_spec_boolean (SHOW_BRIGHTNESS_POPUP,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::handle-brightness-keys
   **/
  g_object_class_install_property (object_class,
                                   PROP_HANDLE_BRIGHTNESS_KEYS,
                                   g_param_spec_boolean (HANDLE_BRIGHTNESS_KEYS,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::brightness-step-count
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_STEP_COUNT,
                                   g_param_spec_uint (BRIGHTNESS_STEP_COUNT,
                                                      NULL, NULL,
                                                      2,
                                                      100,
                                                      10,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::brightness-exponential
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_EXPONENTIAL,
                                   g_param_spec_boolean (BRIGHTNESS_EXPONENTIAL,
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::show-tray-icon
   **/
  g_object_class_install_property (object_class,
                                   PROP_TRAY_ICON,
                                   g_param_spec_uint (SHOW_TRAY_ICON_CFG,
                                                      NULL, NULL,
                                                      SHOW_ICON_ALWAYS,
                                                      NEVER_SHOW_ICON,
                                                      SHOW_ICON_WHEN_BATTERY_PRESENT,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::critical-battery-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_CRITICAL_BATTERY_ACTION,
                                   g_param_spec_uint (CRITICAL_BATT_ACTION_CFG,
                                                      NULL, NULL,
                                                      ESPM_DO_NOTHING,
                                                      ESPM_DO_SHUTDOWN,
                                                      ESPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));
  /**
   * EspmEsconf::power-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_POWER_BUTTON,
                                   g_param_spec_uint (POWER_SWITCH_CFG,
                                                      NULL, NULL,
                                                      ESPM_DO_NOTHING,
                                                      ESPM_DO_SHUTDOWN,
                                                      ESPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::sleep-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_SLEEP_BUTTON,
                                   g_param_spec_uint (SLEEP_SWITCH_CFG,
                                                      NULL, NULL,
                                                      ESPM_DO_NOTHING,
                                                      ESPM_DO_SHUTDOWN,
                                                      ESPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::hibernate-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIBERNATE_BUTTON,
                                   g_param_spec_uint (HIBERNATE_SWITCH_CFG,
                                                      NULL, NULL,
                                                      ESPM_DO_NOTHING,
                                                      ESPM_DO_SHUTDOWN,
                                                      ESPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::battery-switch-action
   **/
  g_object_class_install_property (object_class,
                                   PROP_BATTERY_BUTTON,
                                   g_param_spec_uint (BATTERY_SWITCH_CFG,
                                                      NULL, NULL,
                                                      ESPM_DO_NOTHING,
                                                      ESPM_DO_SHUTDOWN,
                                                      ESPM_DO_NOTHING,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::lid-action-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_LID_ACTION_ON_AC,
                                   g_param_spec_uint (LID_SWITCH_ON_AC_CFG,
                                                      NULL, NULL,
                                                      LID_TRIGGER_NOTHING,
                                                      LID_TRIGGER_LOCK_SCREEN,
                                                      LID_TRIGGER_LOCK_SCREEN,
                                                      G_PARAM_READWRITE));

   /**
   * EspmEsconf::brightness-level-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_LEVEL_ON_AC,
                                   g_param_spec_uint  (BRIGHTNESS_LEVEL_ON_AC,
                                                      NULL, NULL,
                                                      1,
                                                      100,
                                                      80,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::brightness-level-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_LEVEL_ON_BATTERY,
                                   g_param_spec_uint  (BRIGHTNESS_LEVEL_ON_BATTERY,
                                                      NULL, NULL,
                                                      1,
                                                      100,
                                                      20,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::lid-action-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_LID_ACTION_ON_BATTERY,
                                   g_param_spec_uint (LID_SWITCH_ON_BATTERY_CFG,
                                                      NULL, NULL,
                                                      LID_TRIGGER_NOTHING,
                                                      LID_TRIGGER_LOCK_SCREEN,
                                                      LID_TRIGGER_LOCK_SCREEN,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::dpms-enabled
   **/
  g_object_class_install_property (object_class,
                                   PROP_ENABLE_DPMS,
                                   g_param_spec_boolean (DPMS_ENABLED_CFG,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  /**
   * EspmEsconf::dpms-on-ac-sleep
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_ON_AC,
                                   g_param_spec_uint (ON_AC_DPMS_SLEEP,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT16,
                                                      10,
                                                      G_PARAM_READWRITE));
  /**
   * EspmEsconf::dpms-on-ac-off
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_OFF_ON_AC,
                                   g_param_spec_uint (ON_AC_DPMS_OFF,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT16,
                                                      15,
                                                      G_PARAM_READWRITE));
  /**
   * EspmEsconf::dpms-on-battery-sleep
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_ON_BATTERY,
                                   g_param_spec_uint (ON_BATT_DPMS_SLEEP,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT16,
                                                      5,
                                                      G_PARAM_READWRITE));
  /**
   * EspmEsconf::dpms-on-battery-off
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_OFF_ON_BATTERY,
                                   g_param_spec_uint (ON_BATT_DPMS_OFF,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT16,
                                                      10,
                                                      G_PARAM_READWRITE));
  /**
   * EspmEsconf::dpms-sleep-mode
   **/
  g_object_class_install_property (object_class,
                                   PROP_DPMS_SLEEP_MODE,
                                   g_param_spec_string  (DPMS_SLEEP_MODE,
                                                         NULL, NULL,
                                                         "Standby",
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::inactivity-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_ON_AC,
                                   g_param_spec_uint (ON_AC_INACTIVITY_TIMEOUT,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      14,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::inactivity-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_ON_BATTERY,
                                   g_param_spec_uint (ON_BATTERY_INACTIVITY_TIMEOUT,
                                                      NULL, NULL,
                                                      0,
                                                      G_MAXUINT,
                                                      14,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::inactivity-sleep-mode-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_SLEEP_MODE_ON_BATTERY,
                                   g_param_spec_uint (INACTIVITY_SLEEP_MODE_ON_BATTERY,
                                                      NULL, NULL,
                                                      ESPM_DO_SUSPEND,
                                                      ESPM_DO_HIBERNATE,
                                                      ESPM_DO_HIBERNATE,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::inactivity-sleep-mode-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_IDLE_SLEEP_MODE_ON_AC,
                                   g_param_spec_uint (INACTIVITY_SLEEP_MODE_ON_AC,
                                                      NULL, NULL,
                                                      ESPM_DO_SUSPEND,
                                                      ESPM_DO_HIBERNATE,
                                                      ESPM_DO_SUSPEND,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::brightness-on-ac
   **/
  g_object_class_install_property (object_class,
                                   PROP_DIM_ON_AC_TIMEOUT,
                                   g_param_spec_uint (BRIGHTNESS_ON_AC,
                                                      NULL, NULL,
                                                      9,
                                                      G_MAXUINT,
                                                      9,
                                                      G_PARAM_READWRITE));

  /**
   * EspmEsconf::brightness-on-battery
   **/
  g_object_class_install_property (object_class,
                                   PROP_DIM_ON_BATTERY_TIMEOUT,
                                   g_param_spec_uint (BRIGHTNESS_ON_BATTERY,
                                                      NULL, NULL,
                                                      9,
                                                      G_MAXUINT,
                                                      120,
                                                      G_PARAM_READWRITE));


  /**
   * EspmEsconf::brightness-slider-min-level
   **/
  g_object_class_install_property (object_class,
                                   PROP_BRIGHTNESS_SLIDER_MIN_LEVEL,
                                   g_param_spec_int (BRIGHTNESS_SLIDER_MIN_LEVEL,
                                                     NULL, NULL,
                                                     -1,
                                                     G_MAXINT32,
                                                     -1,
                                                     G_PARAM_READWRITE));

#ifdef WITH_NETWORK_MANAGER
  /**
   * EspmEsconf::network-manager-sleep
   **/
  g_object_class_install_property (object_class,
                                   PROP_NETWORK_MANAGER_SLEEP,
                                   g_param_spec_boolean (NETWORK_MANAGER_SLEEP,
                                                         NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE));
#endif

  /**
   * EspmEsconf::logind-handle-power-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_POWER_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_POWER_KEY,
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::logind-handle-suspend-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_SUSPEND_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_SUSPEND_KEY,
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::logind-handle-hibernate-key
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_HIBERNATE_KEY,
                                   g_param_spec_boolean (LOGIND_HANDLE_HIBERNATE_KEY,
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::logind-handle-lid-switch
   **/
  g_object_class_install_property (object_class,
                                   PROP_LOGIND_HANDLE_LID_SWITCH,
                                   g_param_spec_boolean (LOGIND_HANDLE_LID_SWITCH,
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * EspmEsconf::heartbeat-command
   **/
  g_object_class_install_property (object_class,
                                   PROP_HEARTBEAT_COMMAND,
                                   g_param_spec_string  (HEARTBEAT_COMMAND,
                                                         NULL, NULL,
                                                         NULL,
                                                         G_PARAM_READWRITE));
}

static void
espm_esconf_init (EspmEsconf *conf)
{
  GError *error = NULL;
  gboolean channel_valid;
  gboolean lock_screen;

  conf->priv = espm_esconf_get_instance_private (conf);

  conf->priv->values = g_new0 (GValue, N_PROPERTIES);

  if ( !esconf_init (&error) )
  {
    if (error)
    {
      g_critical ("esconf_init failed: %s\n", error->message);
      g_error_free (error);
    }
    channel_valid = FALSE;
  }
  else
  {
    conf->priv->channel = esconf_channel_get (ESPM_CHANNEL);
    conf->priv->session_channel = esconf_channel_get ("expidus1-session");

    /* if expidus1-session is around, sync to it on startup */
    if ( esconf_channel_has_property (conf->priv->session_channel, "/shutdown/LockScreen") )
    {
      lock_screen = esconf_channel_get_bool (conf->priv->session_channel,
                                             "/shutdown/LockScreen",
                                             TRUE);

      ESPM_DEBUG("lock screen %s", lock_screen ? "TRUE" : "FALSE");

      g_object_set (G_OBJECT (conf), LOCK_SCREEN_ON_SLEEP, lock_screen, NULL);
    }

    g_signal_connect (conf->priv->channel, "property-changed",
                      G_CALLBACK (espm_esconf_property_changed_cb), conf);

    /* watch for session's property change so we can stay in sync */
    g_signal_connect (conf->priv->session_channel, "property-changed",
                      G_CALLBACK (espm_xfsession_property_changed_cb), conf);

    channel_valid = TRUE;
  }
  espm_esconf_load (conf, channel_valid);
}

static void
espm_esconf_finalize(GObject *object)
{
  EspmEsconf *conf;
  guint i;

  conf = ESPM_ESCONF (object);

  for ( i = 0; i < N_PROPERTIES; i++)
  {
    if ( G_IS_VALUE (conf->priv->values + i) )
      g_value_unset (conf->priv->values + i);
  }

  g_free (conf->priv->values);

  G_OBJECT_CLASS (espm_esconf_parent_class)->finalize(object);
}

EspmEsconf *
espm_esconf_new (void)
{
  static gpointer espm_esconf_object = NULL;

  if ( G_LIKELY (espm_esconf_object != NULL) )
  {
    g_object_ref (espm_esconf_object);
  }
  else
  {
    espm_esconf_object = g_object_new (ESPM_TYPE_ESCONF, NULL);
    g_object_add_weak_pointer (espm_esconf_object, &espm_esconf_object);
  }

  return ESPM_ESCONF (espm_esconf_object);
}

EsconfChannel *
espm_esconf_get_channel (EspmEsconf *conf)
{
  return conf->priv->channel;
}
