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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <gtk/gtk.h>

#include <libexpidus1util/libexpidus1util.h>

#include <libnotify/notify.h>

#include "espm-common.h"
#include "espm-notify.h"
#include "espm-dbus-monitor.h"

static void espm_notify_finalize   (GObject *object);

static NotifyNotification * espm_notify_new_notification_internal (const gchar *title,
                                                                   const gchar *message,
                                                                   const gchar *icon_name,
                                                                   EspmNotifyUrgency urgency) G_GNUC_MALLOC;

struct EspmNotifyPrivate
{
  EspmDBusMonitor    *monitor;

  NotifyNotification *notification;
  NotifyNotification *critical;

  gulong              critical_id;
  gulong              notify_id;

  gboolean            supports_actions;
  gboolean            supports_sync; /* For x-canonical-private-synchronous */
};

enum
{
  PROP_0,
  PROP_ACTIONS,
  PROP_SYNC
};

G_DEFINE_TYPE_WITH_PRIVATE (EspmNotify, espm_notify, G_TYPE_OBJECT)

static void
espm_notify_get_server_caps (EspmNotify *notify)
{
  GList *caps = NULL;
  notify->priv->supports_actions = FALSE;
  notify->priv->supports_sync    = FALSE;

  caps = notify_get_server_caps ();

  if (caps != NULL)
  {
    if (g_list_find_custom (caps, "x-canonical-private-synchronous", (GCompareFunc) g_strcmp0) != NULL)
      notify->priv->supports_sync = TRUE;

    if (g_list_find_custom (caps, "actions", (GCompareFunc) g_strcmp0) != NULL)
      notify->priv->supports_actions = TRUE;

    g_list_foreach (caps, (GFunc) g_free, NULL);
    g_list_free (caps);
  }
}

static void
espm_notify_check_server (EspmDBusMonitor *monitor,
                          gchar *service_name,
                          gboolean connected,
                          gboolean on_session,
                          EspmNotify *notify)
{
  if ( !g_strcmp0 (service_name, "org.freedesktop.Notifications") && on_session && connected )
    espm_notify_get_server_caps (notify);
}

static void espm_notify_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  EspmNotify *notify;

  notify = ESPM_NOTIFY (object);

  switch (prop_id)
  {
    case PROP_ACTIONS:
      g_value_set_boolean (value, notify->priv->supports_actions);
      break;
    case PROP_SYNC:
      g_value_set_boolean (value, notify->priv->supports_sync);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
espm_notify_class_init (EspmNotifyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = espm_notify_finalize;
  object_class->get_property = espm_notify_get_property;

  g_object_class_install_property (object_class,
                                   PROP_ACTIONS,
                                   g_param_spec_boolean ("actions",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SYNC,
                                   g_param_spec_boolean ("sync",
                                                         NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));
}

static void
espm_notify_init (EspmNotify *notify)
{
  notify->priv = espm_notify_get_instance_private (notify);

  notify->priv->notification = NULL;
  notify->priv->critical = NULL;

  notify->priv->critical_id = 0;
  notify->priv->notify_id   = 0;

  notify->priv->monitor = espm_dbus_monitor_new ();
  espm_dbus_monitor_add_service (notify->priv->monitor, G_BUS_TYPE_SESSION, "org.freedesktop.Notifications");
  g_signal_connect (notify->priv->monitor, "service-connection-changed",
                    G_CALLBACK (espm_notify_check_server), notify);

  espm_notify_get_server_caps (notify);
}

static void
espm_notify_finalize (GObject *object)
{
  EspmNotify *notify;

  notify = ESPM_NOTIFY (object);

  espm_notify_close_normal (notify);
  espm_notify_close_critical (notify);

  G_OBJECT_CLASS (espm_notify_parent_class)->finalize(object);
}

static NotifyNotification *
espm_notify_new_notification_internal (const gchar       *title,
                                       const gchar       *message,
                                       const gchar       *icon_name,
                                       EspmNotifyUrgency  urgency)
{
  NotifyNotification *n;

#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION (0, 7, 0)
  n = notify_notification_new (title, message, icon_name);
#else
  n = notify_notification_new (title, message, icon_name, NULL);
#endif
#else
  n = notify_notification_new (title, message, icon_name, NULL);
#endif

  /* Only set transient hint on non-critical notifications, so that the critical
     ones also end up in the notification server's log */
  if (urgency != ESPM_NOTIFY_CRITICAL)
    notify_notification_set_hint (n, "transient", g_variant_new_boolean (FALSE));

  notify_notification_set_hint (n, "image-path", g_variant_new_string (icon_name));
  notify_notification_set_urgency (n, (NotifyUrgency)urgency);

  return n;
}

static void
espm_notify_close_critical_cb (NotifyNotification *n,
                               EspmNotify         *notify)
{
  notify->priv->critical = NULL;
  g_object_unref (G_OBJECT (n));
}

static gboolean
espm_notify_show (gpointer user_data)
{
  NotifyNotification *n = user_data;
  notify_notification_show (n, NULL);

  return FALSE;
}

static void
espm_notify_close_notification (EspmNotify *notify)
{
  if (notify->priv->notify_id != 0)
  {
    g_source_remove (notify->priv->notify_id);
    notify->priv->notify_id = 0;
  }

  if ( notify->priv->notification )
  {
    if (!notify_notification_close (notify->priv->notification, NULL))
      g_warning ("Failed to close notification\n");

    g_object_unref (G_OBJECT(notify->priv->notification) );
    notify->priv->notification  = NULL;
  }
}

EspmNotify *
espm_notify_new (void)
{
  static gpointer espm_notify_object = NULL;

  if ( espm_notify_object != NULL )
  {
    g_object_ref (espm_notify_object);
  }
  else
  {
    espm_notify_object = g_object_new (ESPM_TYPE_NOTIFY, NULL);
    g_object_add_weak_pointer (espm_notify_object, &espm_notify_object);
  }

  return ESPM_NOTIFY (espm_notify_object);
}

void
espm_notify_show_notification (EspmNotify        *notify,
                               const gchar       *title,
                               const gchar       *text,
                               const gchar       *icon_name,
                               EspmNotifyUrgency  urgency)
{
  NotifyNotification *n;

  espm_notify_close_notification (notify);
  n = espm_notify_new_notification_internal (title, text, icon_name, urgency);
  espm_notify_present_notification (notify, n);
}

NotifyNotification *
espm_notify_new_notification (EspmNotify        *notify,
                              const gchar       *title,
                              const gchar       *text,
                              const gchar       *icon_name,
                              EspmNotifyUrgency  urgency)
{
  NotifyNotification *n = espm_notify_new_notification_internal (title,
                                                                 text, icon_name,
                                                                 urgency);
  return n;
}

void
espm_notify_add_action_to_notification (EspmNotify           *notify,
                                        NotifyNotification   *n,
                                        const gchar          *id,
                                        const gchar          *action_label,
                                        NotifyActionCallback  callback,
                                        gpointer              data)
{
  g_return_if_fail (ESPM_IS_NOTIFY(notify));

  notify_notification_add_action (n, id, action_label,
                                  (NotifyActionCallback)callback,
                                  data, NULL);
}

static void
espm_notify_closed_cb (NotifyNotification *n, EspmNotify *notify)
{
  notify->priv->notification = NULL;
  g_object_unref (G_OBJECT (n));
}

void
espm_notify_present_notification (EspmNotify         *notify,
                                  NotifyNotification *n)
{
  g_return_if_fail (ESPM_IS_NOTIFY(notify));

  espm_notify_close_notification (notify);

  g_signal_connect (G_OBJECT (n),"closed",
                    G_CALLBACK (espm_notify_closed_cb), notify);
                    notify->priv->notification = n;

  notify->priv->notify_id = g_idle_add (espm_notify_show, n);
}

void
espm_notify_critical (EspmNotify         *notify,
                      NotifyNotification *n)
{
  g_return_if_fail (ESPM_IS_NOTIFY (notify));

  espm_notify_close_critical (notify);

  notify->priv->critical = n;

  g_signal_connect (G_OBJECT (n), "closed",
                    G_CALLBACK (espm_notify_close_critical_cb), notify);

  notify->priv->critical_id = g_idle_add (espm_notify_show, n);
}

void
espm_notify_close_critical (EspmNotify *notify)
{
  GError *error = NULL;

  g_return_if_fail (ESPM_IS_NOTIFY (notify));

  if (notify->priv->critical_id != 0)
  {
    g_source_remove (notify->priv->critical_id);
    notify->priv->critical_id = 0;
  }

  if ( notify->priv->critical )
  {
    if (!notify_notification_close (notify->priv->critical, &error))
    {
      if (error)
      {
        g_warning ("Failed to close critical notification: %s", error->message);
        g_error_free (error);
      }
    }

    g_object_unref (G_OBJECT(notify->priv->critical) );
    notify->priv->critical  = NULL;
  }
}

void
espm_notify_close_normal (EspmNotify *notify)
{
  g_return_if_fail (ESPM_IS_NOTIFY (notify));

  espm_notify_close_notification (notify);
}
