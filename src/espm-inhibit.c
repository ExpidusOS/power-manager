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

#include <glib.h>

#include <libexpidus1util/libexpidus1util.h>

#include "espm-inhibit.h"
#include "espm-dbus-monitor.h"
#include "espm-errors.h"
#include "espm-debug.h"

static void espm_inhibit_finalize         (GObject *object);
static void espm_inhibit_dbus_class_init  (EspmInhibitClass *klass);
static void espm_inhibit_dbus_init        (EspmInhibit *inhibit);

struct EspmInhibitPrivate
{
  EspmDBusMonitor *monitor;
  GPtrArray       *array;
  gboolean         inhibited;
};

typedef struct
{
  gchar *app_name;
  gchar *unique_name;
  guint  cookie;
} Inhibitor;

enum
{
  HAS_INHIBIT_CHANGED,
  INHIBIT_LIST_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EspmInhibit, espm_inhibit, G_TYPE_OBJECT)

static void
espm_inhibit_free_inhibitor (EspmInhibit *inhibit, Inhibitor *inhibitor)
{
  g_return_if_fail (inhibitor != NULL );

  g_ptr_array_remove (inhibit->priv->array, inhibitor);

  g_free (inhibitor->app_name);
  g_free (inhibitor->unique_name);
  g_free (inhibitor);
}

static gboolean
espm_inhibit_has_inhibit_changed (EspmInhibit *inhibit)
{
  if ( inhibit->priv->array->len == 0 && inhibit->priv->inhibited == TRUE )
  {
    ESPM_DEBUG("Inhibit removed");
    inhibit->priv->inhibited = FALSE;
    g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
  }
  else if ( inhibit->priv->array->len != 0 && inhibit->priv->inhibited == FALSE )
  {
    ESPM_DEBUG("Inhibit added");
    inhibit->priv->inhibited = TRUE;
    g_signal_emit (G_OBJECT(inhibit), signals[HAS_INHIBIT_CHANGED], 0, inhibit->priv->inhibited);
  }

  /* Always emite the INHIBIT_LIST_CHANGED for any change so the panel plugin
   * stays in sync */
  g_signal_emit (G_OBJECT(inhibit), signals[INHIBIT_LIST_CHANGED], 0, inhibit->priv->inhibited);

  return inhibit->priv->inhibited;
}

static guint
espm_inhibit_get_cookie (EspmInhibit *inhibit)
{
  guint max = 0;
  guint i;
  Inhibitor *inhibitor;

  for ( i = 0; i<inhibit->priv->array->len; i++)
  {
    inhibitor = g_ptr_array_index (inhibit->priv->array, i);
    max = MAX (max, inhibitor->cookie);
  }

  return (guint) g_random_int_range ( max + 1, max + 40);
}

static guint
espm_inhibit_add_application (EspmInhibit *inhibit, const gchar *app_name, const gchar *unique_name)
{
  guint cookie;
  Inhibitor *inhibitor;

  inhibitor = g_new0 (Inhibitor, 1);

  cookie = espm_inhibit_get_cookie (inhibit);

  inhibitor->cookie = cookie;
  inhibitor->app_name = g_strdup (app_name);
  inhibitor->unique_name = g_strdup (unique_name);

  g_ptr_array_add (inhibit->priv->array, inhibitor);

  return cookie;
}

static Inhibitor *
espm_inhibit_find_application_by_cookie (EspmInhibit *inhibit, guint cookie)
{
  guint i;
  Inhibitor *inhibitor;
  for ( i = 0; i < inhibit->priv->array->len; i++)
  {
    inhibitor = g_ptr_array_index (inhibit->priv->array, i);
    if ( inhibitor->cookie == cookie )
    {
      return inhibitor;
    }
  }

  return NULL;
}

static Inhibitor *
espm_inhibit_find_application_by_unique_connection_name (EspmInhibit *inhibit, const gchar *unique_name)
{
  guint i;
  Inhibitor *inhibitor;
  for ( i = 0; i < inhibit->priv->array->len; i++)
  {
    inhibitor = g_ptr_array_index (inhibit->priv->array, i);
    if ( g_strcmp0 (inhibitor->unique_name, unique_name ) == 0 )
    {
      return inhibitor;
    }
  }

  return NULL;
}

static gboolean
espm_inhibit_remove_application_by_cookie (EspmInhibit *inhibit, guint cookie)
{
  Inhibitor *inhibitor;

  inhibitor = espm_inhibit_find_application_by_cookie (inhibit, cookie);

  if ( inhibitor )
  {
    espm_dbus_monitor_remove_unique_name (inhibit->priv->monitor, G_BUS_TYPE_SESSION, inhibitor->unique_name);
    espm_inhibit_free_inhibitor (inhibit, inhibitor);
    return TRUE;
  }
  return FALSE;
}

static void
espm_inhibit_connection_lost_cb (EspmDBusMonitor *monitor, gchar *unique_name,
                                 gboolean on_session, EspmInhibit *inhibit)
{
  Inhibitor *inhibitor;

  if ( !on_session)
    return;

  inhibitor = espm_inhibit_find_application_by_unique_connection_name (inhibit, unique_name );

  if ( inhibitor )
  {
    ESPM_DEBUG ("Application=%s with unique connection name=%s disconnected", inhibitor->app_name, inhibitor->unique_name);
    espm_inhibit_free_inhibitor (inhibit, inhibitor);
    espm_inhibit_has_inhibit_changed (inhibit);
  }
}

static void
espm_inhibit_class_init(EspmInhibitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  signals[HAS_INHIBIT_CHANGED] =
    g_signal_new ("has-inhibit-changed",
                  ESPM_TYPE_INHIBIT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET(EspmInhibitClass, has_inhibit_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[INHIBIT_LIST_CHANGED] =
    g_signal_new ("inhibitors-list-changed",
                  ESPM_TYPE_INHIBIT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET(EspmInhibitClass, inhibitors_list_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  object_class->finalize = espm_inhibit_finalize;

  espm_inhibit_dbus_class_init (klass);
}

static void
espm_inhibit_init (EspmInhibit *inhibit)
{
  inhibit->priv = espm_inhibit_get_instance_private (inhibit);

  inhibit->priv->array   = g_ptr_array_new ();
  inhibit->priv->monitor = espm_dbus_monitor_new ();

  g_signal_connect (inhibit->priv->monitor, "unique-name-lost",
                    G_CALLBACK (espm_inhibit_connection_lost_cb), inhibit);

  espm_inhibit_dbus_init (inhibit);
}

static void
espm_inhibit_finalize (GObject *object)
{
  EspmInhibit *inhibit;
  Inhibitor *inhibitor;
  guint i;

  inhibit = ESPM_INHIBIT(object);

  g_object_unref (inhibit->priv->monitor);

  for ( i = 0; i<inhibit->priv->array->len; i++)
  {
    inhibitor = g_ptr_array_index (inhibit->priv->array, i);
    espm_inhibit_free_inhibitor (inhibit, inhibitor);
  }

  g_ptr_array_free (inhibit->priv->array, TRUE);

  G_OBJECT_CLASS (espm_inhibit_parent_class)->finalize(object);
}

EspmInhibit *
espm_inhibit_new (void)
{
  static gpointer espm_inhibit_object = NULL;

  if ( G_LIKELY (espm_inhibit_object != NULL) )
  {
    g_object_ref (espm_inhibit_object);
  }
  else
  {
    espm_inhibit_object = g_object_new (ESPM_TYPE_INHIBIT, NULL);
    g_object_add_weak_pointer (espm_inhibit_object, &espm_inhibit_object);
  }
  return ESPM_INHIBIT (espm_inhibit_object);
}

/***
 * espm_inhibit_get_inhibit_list
 * @inhibit: the EspmInhibit object.
 *
 * Returns: An array of applications that are currently inhibiting espm.
 *          Call g_free on the returned array when done using it.
 */
const gchar **
espm_inhibit_get_inhibit_list (EspmInhibit *inhibit)
{
  guint i;
  Inhibitor *inhibitor;
  const gchar **OUT_inhibitors;

  ESPM_DEBUG ("entering espm_inhibit_get_inhibit_list");

  OUT_inhibitors = g_new (const gchar *, inhibit->priv->array->len + 1);

  for ( i = 0; i<inhibit->priv->array->len; i++)
  {
    inhibitor = g_ptr_array_index (inhibit->priv->array, i);
    OUT_inhibitors[i] = inhibitor->app_name;
  }

  OUT_inhibitors[inhibit->priv->array->len] = NULL;

  return OUT_inhibitors;
}

/*
 *
 * DBus server implementation for org.freedesktop.PowerManagement.Inhibit
 *
 */
static gboolean espm_inhibit_inhibit (EspmInhibit *inhibit,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *IN_appname,
                                       const gchar *IN_reason,
                                       gpointer user_data);

static gboolean espm_inhibit_un_inhibit (EspmInhibit *inhibit,
                                         GDBusMethodInvocation *invocation,
                                         guint IN_cookie,
                                         gpointer user_data);

static gboolean espm_inhibit_has_inhibit (EspmInhibit *inhibit,
                                          GDBusMethodInvocation *invocation,
                                          gpointer user_data);

static gboolean espm_inhibit_get_inhibitors (EspmInhibit *inhibit,
                                             GDBusMethodInvocation *invocation,
                                             gpointer user_data);

#include "org.freedesktop.PowerManagement.Inhibit.h"

static void
espm_inhibit_dbus_class_init (EspmInhibitClass *klass)
{
}

static void
espm_inhibit_dbus_init (EspmInhibit *inhibit)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  EspmPowerManagementInhibit *inhibit_dbus;

  inhibit_dbus = espm_power_management_inhibit_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (inhibit_dbus),
                                    bus,
                                    "/org/freedesktop/PowerManagement/Inhibit",
                                    NULL);

  g_signal_connect_swapped (inhibit_dbus,
                            "handle-inhibit",
                            G_CALLBACK (espm_inhibit_inhibit),
                            inhibit);
  g_signal_connect_swapped (inhibit_dbus,
                            "handle-un-inhibit",
                            G_CALLBACK (espm_inhibit_un_inhibit),
                            inhibit);
  g_signal_connect_swapped (inhibit_dbus,
                            "handle-has-inhibit",
                            G_CALLBACK (espm_inhibit_has_inhibit),
                            inhibit);
  g_signal_connect_swapped (inhibit_dbus,
                            "handle-get-inhibitors",
                            G_CALLBACK (espm_inhibit_get_inhibitors),
                            inhibit);
}

static gboolean
espm_inhibit_inhibit (EspmInhibit *inhibit,
                      GDBusMethodInvocation *invocation,
                      const gchar *IN_appname,
                      const gchar *IN_reason,
                      gpointer user_data)
{
  const gchar *sender;
  guint cookie;

  if ( IN_appname == NULL || IN_reason == NULL )
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_INVALID_ARGUMENTS,
                                           _("Invalid arguments"));

    return TRUE;
  }

  sender = g_dbus_method_invocation_get_sender (invocation);
  cookie = espm_inhibit_add_application (inhibit, IN_appname, sender);

  ESPM_DEBUG("Inhibit send application name=%s reason=%s sender=%s", IN_appname, IN_reason ,sender);

  espm_inhibit_has_inhibit_changed (inhibit);

  espm_dbus_monitor_add_unique_name (inhibit->priv->monitor, G_BUS_TYPE_SESSION, sender);

  espm_power_management_inhibit_complete_inhibit (user_data,
                                                  invocation,
                                                  cookie);

  return TRUE;
}

static gboolean
espm_inhibit_un_inhibit (EspmInhibit *inhibit,
                         GDBusMethodInvocation *invocation,
                         guint IN_cookie,
                         gpointer user_data)
{
  ESPM_DEBUG("UnHibit message received");

  if (!espm_inhibit_remove_application_by_cookie (inhibit, IN_cookie))
  {
    g_dbus_method_invocation_return_error (invocation,
                                           ESPM_ERROR,
                                           ESPM_ERROR_COOKIE_NOT_FOUND,
                                           _("Invalid cookie"));
    return TRUE;
  }

  espm_inhibit_has_inhibit_changed (inhibit);

  espm_power_management_inhibit_complete_un_inhibit (user_data, invocation);

  return TRUE;
}

static gboolean
espm_inhibit_has_inhibit (EspmInhibit *inhibit,
                          GDBusMethodInvocation *invocation,
                          gpointer user_data)
{
  ESPM_DEBUG("Has Inhibit message received");

  espm_power_management_inhibit_complete_has_inhibit (user_data,
                                                      invocation,
                                                      inhibit->priv->inhibited);

  return TRUE;
}

static gboolean
espm_inhibit_get_inhibitors (EspmInhibit *inhibit,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
  const gchar **OUT_inhibitors;

  ESPM_DEBUG ("Get Inhibitors message received");

  OUT_inhibitors = espm_inhibit_get_inhibit_list (inhibit);

  espm_power_management_inhibit_complete_get_inhibitors (user_data,
                                                         invocation,
                                                         OUT_inhibitors);

  g_free (OUT_inhibitors);

  return TRUE;
}
