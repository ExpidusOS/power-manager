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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libexpidus1util/libexpidus1util.h>
#include <libexpidus1ui/libexpidus1ui.h>

#include "espm-dbus.h"
#include "espm-debug.h"
#include "espm-common.h"

#include "expidus-power-manager-dbus.h"
#include "espm-manager.h"

static void G_GNUC_NORETURN
show_version (void)
{
  g_print (_("\n"
           "Expidus Power Manager %s\n\n"
           "Part of the Expidus Goodies Project\n"
           "http://goodies.expidus.org\n\n"
           "Licensed under the GNU GPL.\n\n"), VERSION);

  exit (EXIT_SUCCESS);
}

static void
espm_quit_signal (gint sig, gpointer data)
{
  EspmManager *manager = (EspmManager *) data;

  ESPM_DEBUG ("sig %d", sig);

  if ( sig != SIGHUP )
    espm_manager_stop (manager);
}

static const gchar *
espm_bool_to_local_string (gboolean value)
{
  return value == TRUE ? _("True") : _("False");
}

static void
espm_dump (GHashTable *hash)
{
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

  g_print ("---------------------------------------------------\n");
  g_print ("       Expidus power manager version %s\n", VERSION);
#ifdef ENABLE_POLKIT
  g_print (_("With policykit support\n"));
#else
  g_print (_("Without policykit support\n"));
#endif
#ifdef WITH_NETWORK_MANAGER
  g_print (_("With network manager support\n"));
#else
  g_print (_("Without network manager support\n"));
#endif
  g_print ("---------------------------------------------------\n");
  g_print ( "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n"
            "%s: %s\n",
           _("Can suspend"),
           espm_bool_to_local_string (can_suspend),
           _("Can hibernate"),
           espm_bool_to_local_string (can_hibernate),
           _("Authorized to suspend"),
           espm_bool_to_local_string (auth_suspend),
           _("Authorized to hibernate"),
           espm_bool_to_local_string (auth_hibernate),
           _("Authorized to shutdown"),
           espm_bool_to_local_string (can_shutdown),
           _("Has battery"),
           espm_bool_to_local_string (has_battery),
           _("Has brightness panel"),
           espm_bool_to_local_string (has_lcd_brightness),
           _("Has power button"),
           espm_bool_to_local_string (has_power_button),
           _("Has hibernate button"),
           espm_bool_to_local_string (has_hibernate_button),
           _("Has sleep button"),
            espm_bool_to_local_string (has_sleep_button),
                 _("Has battery button"),
                  espm_bool_to_local_string (has_battery_button),
           _("Has LID"),
            espm_bool_to_local_string (has_lid));
}

static void
espm_dump_remote (GDBusConnection *bus)
{
  EspmPowerManager *proxy;
  GError *error = NULL;
  GVariant *config;
  GVariantIter *iter;
  GHashTable *hash;
  gchar *key, *value;

  proxy = espm_power_manager_proxy_new_sync (bus,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                             G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                             "com.expidus.PowerManager",
                                             "/org/expidus/PowerManager",
                                             NULL,
                                             NULL);

  espm_power_manager_call_get_config_sync (proxy,
                                           &config,
                                           NULL,
                                           &error);

  g_object_unref (proxy);

  if ( error )
  {
    g_error ("%s", error->message);
    exit (EXIT_FAILURE);
  }

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_variant_get (config, "a{ss}", &iter);
  while (g_variant_iter_next (iter, "{ss}", &key, &value))
  {
    g_hash_table_insert (hash, key, value);
  }
  g_variant_iter_free (iter);
  g_variant_unref (config);

  espm_dump (hash);
  g_hash_table_destroy (hash);
}

static void G_GNUC_NORETURN
espm_start (GDBusConnection *bus, const gchar *client_id, gboolean dump)
{
  EspmManager *manager;
  GError *error = NULL;

  ESPM_DEBUG ("Starting the power manager");

  manager = espm_manager_new (bus, client_id);

  if ( expidus_posix_signal_handler_init (&error))
  {
    expidus_posix_signal_handler_set_handler (SIGHUP,
                                           espm_quit_signal,
                                           manager, NULL);

    expidus_posix_signal_handler_set_handler (SIGINT,
                                           espm_quit_signal,
             manager, NULL);

    expidus_posix_signal_handler_set_handler (SIGTERM,
                                           espm_quit_signal,
                                           manager, NULL);
  }
  else
  {
    if (error)
    {
      g_warning ("Unable to set up POSIX signal handlers: %s", error->message);
      g_error_free (error);
    }
  }

  espm_manager_start (manager);

  if ( dump )
  {
    GHashTable *hash;
    hash = espm_manager_get_config (manager);
    espm_dump (hash);
    g_hash_table_destroy (hash);
  }


  gtk_main ();

  g_object_unref (manager);

  exit (EXIT_SUCCESS);
}

int main (int argc, char **argv)
{
  GDBusConnection *bus;
  GError *error = NULL;
  EspmPowerManager *proxy;
  GOptionContext *octx;

  gboolean run        = FALSE;
  gboolean quit       = FALSE;
  gboolean config     = FALSE;
  gboolean version    = FALSE;
  gboolean reload     = FALSE;
  gboolean daemonize  = FALSE;
  gboolean debug      = FALSE;
  gboolean dump       = FALSE;
  gchar   *client_id  = NULL;

  GOptionEntry option_entries[] =
  {
    { "run",'r', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &run, NULL, NULL },
    { "daemon",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &daemonize, N_("Daemonize"), NULL },
    { "debug",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &debug, N_("Enable debugging"), NULL },
    { "dump",'\0' , G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &dump, N_("Dump all information"), NULL },
    { "restart", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &reload, N_("Restart the running instance of Expidus power manager"), NULL},
    { "customize", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &config, N_("Show the configuration dialog"), NULL },
    { "quit", 'q', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &quit, N_("Quit any running expidus power manager"), NULL },
    { "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version, N_("Version information"), NULL },
    { "sm-client-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &client_id, NULL, NULL },
    { NULL, },
  };

  /* Parse the options */
  octx = g_option_context_new("");
  g_option_context_set_ignore_unknown_options(octx, TRUE);
  g_option_context_add_main_entries(octx, option_entries, NULL);
  g_option_context_add_group(octx, expidus_sm_client_get_option_group(argc, argv));
  /* We can't add the following command because it will invoke gtk_init
     before we have a chance to fork.
     g_option_context_add_group(octx, gtk_get_option_group(TRUE));
   */

  if (!g_option_context_parse(octx, &argc, &argv, &error))
  {
    if (error)
    {
      g_printerr(_("Failed to parse arguments: %s\n"), error->message);
      g_error_free(error);
    }
    g_option_context_free(octx);

    return EXIT_FAILURE;
  }

  g_option_context_free(octx);

  if ( version )
    show_version ();

  /* Fork if needed */
  if ( dump == FALSE && debug == FALSE && daemonize == TRUE && daemon(0,0) )
  {
    g_critical ("Could not daemonize");
  }

  /* Initialize */
  expidus_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  g_set_application_name (PACKAGE_NAME);

  if (!gtk_init_check (&argc, &argv))
  {
    if (G_LIKELY (error))
    {
      g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
      g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
      g_printerr ("\n");
      g_error_free (error);
    }
    else
    {
      g_error ("Unable to open display.");
    }

    return EXIT_FAILURE;
  }

  espm_debug_init (debug);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if ( error )
  {
    expidus_dialog_show_error (NULL,
                            error,
                            "%s",
                            _("Unable to get connection to the message bus session"));
    g_error ("%s: \n", error->message);
  }

  if ( quit )
  {
    if (!espm_dbus_name_has_owner (bus, "com.expidus.PowerManager") )
    {
      g_print (_("Expidus power manager is not running"));
      g_print ("\n");
      return EXIT_SUCCESS;
    }
    else
    {
      proxy = espm_power_manager_proxy_new_sync (bus,
                   G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                   G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                   "com.expidus.PowerManager",
                   "/org/expidus/PowerManager",
                   NULL,
                   NULL);
      if ( !proxy )
      {
        g_critical ("Failed to get proxy");
        g_object_unref(bus);
        return EXIT_FAILURE;
      }
      espm_power_manager_call_quit_sync (proxy, NULL, &error);
      g_object_unref (proxy);

      if ( error)
      {
        g_critical ("Failed to send quit message %s:\n", error->message);
        g_error_free (error);
      }
    }
    return EXIT_SUCCESS;
  }

  if ( config )
  {
    g_spawn_command_line_async ("expidus1-power-manager-settings", &error);

    if ( error )
    {
        g_critical ("Failed to execute expidus1-power-manager-settings: %s", error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if ( reload )
  {
    if (!espm_dbus_name_has_owner (bus, "com.expidus.PowerManager") &&
        !espm_dbus_name_has_owner (bus, "org.freedesktop.PowerManagement"))
    {
      g_print ("Expidus power manager is not running\n");
      espm_start (bus, client_id, dump);
    }

    proxy = espm_power_manager_proxy_new_sync (bus,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               "com.expidus.PowerManager",
                                               "/org/expidus/PowerManager",
                                               NULL,
                                               NULL);
    if ( !proxy )
    {
      g_critical ("Failed to get proxy");
      g_object_unref (bus);
      return EXIT_FAILURE;
    }

    if ( !espm_power_manager_call_restart_sync (proxy, NULL, NULL) )
    {
      g_critical ("Unable to send reload message");
      g_object_unref (proxy);
      g_object_unref (bus);
      return EXIT_SUCCESS;
    }
    return EXIT_SUCCESS;
  }

  if (dump)
  {
    if (espm_dbus_name_has_owner (bus, "com.expidus.PowerManager"))
    {
      espm_dump_remote (bus);
      return EXIT_SUCCESS;
    }
  }

  if (espm_dbus_name_has_owner (bus, "org.freedesktop.PowerManagement") )
  {
    g_print ("%s: %s\n",
             _("Expidus Power Manager"),
             _("Another power manager is already running"));
  }
  else if (espm_dbus_name_has_owner (bus, "com.expidus.PowerManager"))
  {
    g_print (_("Expidus power manager is already running"));
    g_print ("\n");
    return EXIT_SUCCESS;
  }
  else
  {
    espm_start (bus, client_id, dump);
  }

  return EXIT_SUCCESS;
}
