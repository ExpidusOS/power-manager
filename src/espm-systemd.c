/*
 * * Copyright (C) 2009-2011 Ali <aliov@expidus.org>
 * * Copyright (C) 2013 Andreas Müller <schnitzeltony@googlemail.com>
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

#include <gio/gio.h>

#include "espm-systemd.h"
#include "espm-polkit.h"

static void espm_systemd_finalize   (GObject *object);

static void espm_systemd_get_property (GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec);

struct EspmSystemdPrivate
{
    gboolean         can_shutdown;
    gboolean         can_restart;
    gboolean         can_suspend;
    gboolean         can_hibernate;
#ifdef ENABLE_POLKIT
    EspmPolkit      *polkit;
#endif
};

enum
{
    PROP_0,
    PROP_CAN_RESTART,
    PROP_CAN_SHUTDOWN,
    PROP_CAN_SUSPEND,
    PROP_CAN_HIBERNATE,
};

G_DEFINE_TYPE_WITH_PRIVATE (EspmSystemd, espm_systemd, G_TYPE_OBJECT)

#define SYSTEMD_DBUS_NAME               "org.freedesktop.login1"
#define SYSTEMD_DBUS_PATH               "/org/freedesktop/login1"
#define SYSTEMD_DBUS_INTERFACE          "org.freedesktop.login1.Manager"
#define SYSTEMD_REBOOT_ACTION           "Reboot"
#define SYSTEMD_POWEROFF_ACTION         "PowerOff"
#define SYSTEMD_REBOOT_TEST             "org.freedesktop.login1.reboot"
#define SYSTEMD_POWEROFF_TEST           "org.freedesktop.login1.power-off"
#define SYSTEMD_SUSPEND_TEST            "org.freedesktop.login1.suspend"
#define SYSTEMD_HIBERNATE_TEST          "org.freedesktop.login1.hibernate"

static void
espm_systemd_class_init (EspmSystemdClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = espm_systemd_finalize;

    object_class->get_property = espm_systemd_get_property;

    g_object_class_install_property (object_class,
                                     PROP_CAN_RESTART,
                                     g_param_spec_boolean ("can-restart",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));

    g_object_class_install_property (object_class,
                                     PROP_CAN_SHUTDOWN,
                                     g_param_spec_boolean ("can-shutdown",
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
                                     PROP_CAN_HIBERNATE,
                                     g_param_spec_boolean ("can-hibernate",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));
}

static gboolean
espm_systemd_can_method (EspmSystemd  *systemd,
                         gboolean     *can_method,
                         const gchar  *method)
{
    *can_method = FALSE;

#ifdef ENABLE_POLKIT
    *can_method = espm_polkit_check_auth(systemd->priv->polkit, method);

    return TRUE;
#endif

    return FALSE;
}

static void
espm_systemd_init (EspmSystemd *systemd)
{
    systemd->priv = espm_systemd_get_instance_private (systemd);
    systemd->priv->can_shutdown = FALSE;
    systemd->priv->can_restart  = FALSE;
#ifdef ENABLE_POLKIT
    systemd->priv->polkit = espm_polkit_get();
#endif

    espm_systemd_can_method (systemd,
                             &systemd->priv->can_shutdown,
                             SYSTEMD_POWEROFF_TEST);
    espm_systemd_can_method (systemd,
                             &systemd->priv->can_restart,
                             SYSTEMD_REBOOT_TEST);
    espm_systemd_can_method (systemd,
                             &systemd->priv->can_suspend,
                             SYSTEMD_SUSPEND_TEST);
    espm_systemd_can_method (systemd,
                             &systemd->priv->can_hibernate,
                             SYSTEMD_HIBERNATE_TEST);
}

static void espm_systemd_get_property (GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    EspmSystemd *systemd;
    systemd = ESPM_SYSTEMD (object);

    switch (prop_id)
    {
    case PROP_CAN_SHUTDOWN:
        g_value_set_boolean (value, systemd->priv->can_shutdown);
        break;
    case PROP_CAN_RESTART:
        g_value_set_boolean (value, systemd->priv->can_restart);
        break;
    case PROP_CAN_SUSPEND:
        g_value_set_boolean (value, systemd->priv->can_suspend);
        break;
    case PROP_CAN_HIBERNATE:
        g_value_set_boolean (value, systemd->priv->can_hibernate);
        break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
espm_systemd_finalize (GObject *object)
{
#ifdef ENABLE_POLKIT
    EspmSystemd *systemd;

    systemd = ESPM_SYSTEMD (object);

    if(systemd->priv->polkit)
    {
        g_object_unref (G_OBJECT (systemd->priv->polkit));
        systemd->priv->polkit = NULL;
    }
#endif

    G_OBJECT_CLASS (espm_systemd_parent_class)->finalize (object);
}

EspmSystemd *
espm_systemd_new (void)
{
    static gpointer systemd_obj = NULL;

    if ( G_LIKELY (systemd_obj != NULL ) )
    {
        g_object_ref (systemd_obj);
    }
    else
    {
        systemd_obj = g_object_new (ESPM_TYPE_SYSTEMD, NULL);
        g_object_add_weak_pointer (systemd_obj, &systemd_obj);
    }

    return ESPM_SYSTEMD (systemd_obj);
}

static void
espm_systemd_try_method (EspmSystemd  *systemd,
                         const gchar  *method,
                         GError      **error)
{
    GDBusConnection *bus;
    GError          *local_error = NULL;
    GVariant        *var;

    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
    if (G_LIKELY (bus != NULL))
    {
        var = g_dbus_connection_call_sync (bus,
                                           SYSTEMD_DBUS_NAME,
                                           SYSTEMD_DBUS_PATH,
                                           SYSTEMD_DBUS_INTERFACE,
                                           method,
                                           g_variant_new ("(b)", TRUE),
                                           G_VARIANT_TYPE ("()"), 0, G_MAXINT, NULL,
                                           &local_error);
        g_variant_unref (var);
        g_object_unref (G_OBJECT (bus));

        if (local_error != NULL)
        {
            g_propagate_error (error, local_error);
        }
    }
}

void espm_systemd_shutdown (EspmSystemd *systemd, GError **error)
{
    espm_systemd_try_method (systemd,
                             SYSTEMD_POWEROFF_ACTION,
                             error);
}

void espm_systemd_reboot (EspmSystemd *systemd, GError **error)
{
    espm_systemd_try_method (systemd,
                             SYSTEMD_REBOOT_ACTION,
                             error);
}

void espm_systemd_sleep (EspmSystemd *systemd,
                         const gchar *method,
                         GError **error)
{
    espm_systemd_try_method (systemd, method, error);
}
