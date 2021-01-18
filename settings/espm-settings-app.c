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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <glib.h>
#include <gio/gio.h>

#include <esconf/esconf.h>
#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include "espm-settings-app.h"
#include "espm-settings.h"
#include "espm-debug.h"
#include "espm-config.h"
#include "espm-common.h"


struct _EspmSettingsAppPrivate
{
  gboolean          debug;
  Window            socket_id;
  gchar            *device_id;
};

static void espm_settings_app_launch     (GApplication *app);

static void activate_socket              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_device              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_debug               (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_window              (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);
static void activate_quit                (GSimpleAction  *action,
                                          GVariant       *parameter,
                                          gpointer        data);

G_DEFINE_TYPE_WITH_PRIVATE(EspmSettingsApp, espm_settings_app, GTK_TYPE_APPLICATION);



static void
espm_settings_app_init (EspmSettingsApp *app)
{
  const GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,    NULL, N_("Settings manager socket"), N_("SOCKET ID") },
    { "device-id", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, NULL, N_("Display a specific device by UpDevice object path"), N_("UpDevice object path") },
    { "debug",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Enable debugging"), NULL },
    { "version",   'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Display version information"), NULL },
    { "quit",      'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   NULL, N_("Cause expidus1-power-manager-settings to quit"), NULL },
    { NULL, },
  };

  g_application_add_main_option_entries (G_APPLICATION (app), option_entries);
}

static void
espm_settings_app_startup (GApplication *app)
{
  const GActionEntry action_entries[] = {
    { "socket-id", activate_socket, "i"  },
    { "device-id", activate_device, "s"  },
    { "debug",     activate_debug,  NULL },
    { "activate",  activate_window, NULL },
    { "quit",      activate_quit,   NULL },
  };

  TRACE ("entering");

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   app);

  /* keep the app running until we've launched our window */
  g_application_hold (app);

  /* let the parent class do it's startup as well */
  G_APPLICATION_CLASS(espm_settings_app_parent_class)->startup(app);
}

static void
espm_settings_app_activate (GApplication *app)
{
  TRACE ("entering");
}

static void
espm_settings_app_launch (GApplication *app)
{
  EspmSettingsAppPrivate *priv = espm_settings_app_get_instance_private (ESPM_SETTINGS_APP (app));

  EspmPowerManager *manager;
  EsconfChannel    *channel;
  GError           *error = NULL;
  GtkWidget        *dialog;
  GHashTable       *hash;
  GVariant         *config;
  GVariantIter     *iter;
  gchar            *key, *value;
  GList            *windows;

  gboolean has_battery;
  gboolean auth_suspend;
  gboolean auth_hibernate;
  gboolean can_suspend;
  gboolean can_hibernate;
  gboolean can_shutdown;
  gboolean has_lcd_brightness;
  gboolean has_sleep_button;
  gboolean has_hibernate_button;
  gboolean has_power_button;
  gboolean has_battery_button;
  gboolean has_lid;
  gint     start_espm_if_not_running;

  TRACE ("entering");

  windows = gtk_application_get_windows (GTK_APPLICATION (app));

  if (windows != NULL)
  {
    ESPM_DEBUG ("window already opened");

    gdk_notify_startup_complete ();

    if (priv->device_id != NULL)
    {
      espm_settings_show_device_id (priv->device_id);
    }

    return;
  }

  manager = espm_power_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_BUS_NAME_OWNER_FLAGS_NONE,
                                                       "com.expidus.PowerManager",
                                                       "/org/expidus/PowerManager",
                                                       NULL,
                                                       &error);

  if (error != NULL)
  {
    g_critical("espm_power_manager_proxy_new_sync failed: %s\n", error->message);
    expidus_dialog_show_warning (NULL,
                             _("Expidus Power Manager"),
                             "%s",
                             _("Failed to connect to power manager"));
    g_clear_error (&error);
    return;
  }


  while ( !espm_power_manager_call_get_config_sync (manager, &config, NULL, NULL) )
  {
    GtkWidget *startw;

    startw = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO,
                                     _("Expidus1 Power Manager is not running, do you want to launch it now?"));
    start_espm_if_not_running = gtk_dialog_run (GTK_DIALOG (startw));
    gtk_widget_destroy (startw);

    if (start_espm_if_not_running == GTK_RESPONSE_YES)
    {
      GAppInfo *app_info;

      app_info = g_app_info_create_from_commandline ("expidus1-power-manager", "Expidus1 Power Manager",
                                                     G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, NULL);
      if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
          if (error != NULL) {
            g_warning ("expidus1-power-manager could not be launched. %s", error->message);
            g_error_free (error);
            error = NULL;
          }
      }
      /* wait 2 seconds for espm to startup */
      g_usleep ( 2 * 1000000 );
    }
    else
    {
      /* exit without starting espm */
      return;
    }
  }

  if ( !esconf_init(&error) )
  {
    g_critical("esconf init failed: %s using default settings\n", error->message);
    expidus_dialog_show_warning (NULL,
                              _("Expidus Power Manager"),
                              "%s",
                              _("Failed to load power manager configuration, using defaults"));
    g_clear_error (&error);
  }


  channel = esconf_channel_new(ESPM_CHANNEL);

  espm_debug_init (priv->debug);

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_variant_get (config, "a{ss}", &iter);
  while (g_variant_iter_next (iter, "{ss}", &key, &value))
  {
    g_hash_table_insert (hash, key, value);
  }
  g_variant_iter_free (iter);
  g_variant_unref (config);


  has_battery = espm_string_to_bool (g_hash_table_lookup (hash, "has-battery"));
  has_lid = espm_string_to_bool (g_hash_table_lookup (hash, "has-lid"));
  can_suspend = espm_string_to_bool (g_hash_table_lookup (hash, "can-suspend"));
  can_hibernate = espm_string_to_bool (g_hash_table_lookup (hash, "can-hibernate"));
  auth_suspend = espm_string_to_bool (g_hash_table_lookup (hash, "auth-suspend"));
  auth_hibernate = espm_string_to_bool (g_hash_table_lookup (hash, "auth-hibernate"));
  has_lcd_brightness = espm_string_to_bool (g_hash_table_lookup (hash, "has-brightness"));
  has_sleep_button = espm_string_to_bool (g_hash_table_lookup (hash, "sleep-button"));
  has_power_button = espm_string_to_bool (g_hash_table_lookup (hash, "power-button"));
  has_hibernate_button = espm_string_to_bool (g_hash_table_lookup (hash, "hibernate-button"));
  has_battery_button = espm_string_to_bool (g_hash_table_lookup (hash, "battery-button"));
  can_shutdown = espm_string_to_bool (g_hash_table_lookup (hash, "can-shutdown"));

  DBG("socket_id %i", (int)priv->socket_id);
  DBG("device id %s", priv->device_id);

  dialog = espm_settings_dialog_new (channel, auth_suspend, auth_hibernate,
                                     can_suspend, can_hibernate, can_shutdown, has_battery, has_lcd_brightness,
                                     has_lid, has_sleep_button, has_hibernate_button, has_power_button, has_battery_button,
                                     priv->socket_id, priv->device_id, GTK_APPLICATION (app));

  g_hash_table_destroy (hash);

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (dialog));
  g_application_release (app);

  g_object_unref (manager);
}

static void
activate_socket (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
  EspmSettingsApp *app = ESPM_SETTINGS_APP (data);
  EspmSettingsAppPrivate *priv = espm_settings_app_get_instance_private (app);

  TRACE ("entering");

  priv->socket_id = g_variant_get_int32 (parameter);

  espm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_device (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
  EspmSettingsApp *app = ESPM_SETTINGS_APP (data);
  EspmSettingsAppPrivate *priv = espm_settings_app_get_instance_private (app);

  TRACE ("entering");

  priv->device_id = g_strdup(g_variant_get_string (parameter, NULL));

  espm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_debug (GSimpleAction  *action,
                GVariant       *parameter,
                gpointer        data)
{
  EspmSettingsApp *app = ESPM_SETTINGS_APP (data);
  EspmSettingsAppPrivate *priv = espm_settings_app_get_instance_private (app);

  TRACE ("entering");

  priv->debug = TRUE;

  espm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_window (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        data)
{
  EspmSettingsApp *app = ESPM_SETTINGS_APP (data);

  TRACE ("entering");

  espm_settings_app_launch (G_APPLICATION (app));
}

static void
activate_quit (GSimpleAction  *action,
               GVariant       *parameter,
               gpointer        data)
{
  GtkApplication *app = GTK_APPLICATION (data);
  GList *windows;

  TRACE ("entering");

  windows = gtk_application_get_windows (app);

  if (windows)
  {
    /* Remove our window if we've attached one */
    gtk_application_remove_window (app, GTK_WINDOW (windows->data));
  }
}

static gboolean
espm_settings_app_local_options (GApplication *g_application,
                                 GVariantDict *options)
{
  TRACE ("entering");

  /* --version */
  if (g_variant_dict_contains (options, "version"))
  {
    g_print(_("This is %s version %s, running on Expidus %s.\n"), PACKAGE,
            VERSION, expidus_version_string());
    g_print(_("Built with GTK+ %d.%d.%d, linked with GTK+ %d.%d.%d."),
            GTK_MAJOR_VERSION,GTK_MINOR_VERSION, GTK_MICRO_VERSION,
            gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print("\n");

    return 0;
  }

  /* This will call espm_settings_app_startup if it needs to */
  g_application_register (g_application, NULL, NULL);

  /* --debug */
  if (g_variant_dict_contains (options, "debug"))
  {
    g_action_group_activate_action(G_ACTION_GROUP(g_application), "debug", NULL);
    return 0;
  }

  /* --socket-id */
  if (g_variant_dict_contains (options, "socket-id") || g_variant_dict_contains (options, "s"))
  {
    GVariant *var;

    var = g_variant_dict_lookup_value (options, "socket-id", G_VARIANT_TYPE_INT32);

    g_action_group_activate_action(G_ACTION_GROUP(g_application), "socket-id", var);
    return 0;
  }

  /* --device-id */
  if (g_variant_dict_contains (options, "device-id") || g_variant_dict_contains (options, "d"))
  {
    GVariant *var;

    var = g_variant_dict_lookup_value (options, "device-id", G_VARIANT_TYPE_STRING);

    g_action_group_activate_action(G_ACTION_GROUP(g_application), "device-id", var);
    return 0;
  }

  /* --quit */
  if (g_variant_dict_contains (options, "quit") || g_variant_dict_contains (options, "q"))
  {
    g_action_group_activate_action(G_ACTION_GROUP(g_application), "quit", NULL);
    return 0;
  }

  /* default action */
  g_action_group_activate_action(G_ACTION_GROUP(g_application), "activate", NULL);

  return 0;
}

static void
espm_settings_app_class_init (EspmSettingsAppClass *class)
{
  GApplicationClass *gapplication_class = G_APPLICATION_CLASS (class);

  gapplication_class->handle_local_options = espm_settings_app_local_options;
  gapplication_class->startup              = espm_settings_app_startup;
  gapplication_class->activate             = espm_settings_app_activate;
}

EspmSettingsApp *
espm_settings_app_new (void)
{
  return g_object_new (ESPM_TYPE_SETTINGS_APP,
                       "application-id", "com.expidus.PowerManager.Settings",
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
