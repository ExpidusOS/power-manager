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

#ifndef __ESPM_NOTIFY_H
#define __ESPM_NOTIFY_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include <libnotify/notify.h>

G_BEGIN_DECLS

#define ESPM_TYPE_NOTIFY        (espm_notify_get_type () )
#define ESPM_NOTIFY(o)          (G_TYPE_CHECK_INSTANCE_CAST((o), ESPM_TYPE_NOTIFY, EspmNotify))
#define ESPM_IS_NOTIFY(o)       (G_TYPE_CHECK_INSTANCE_TYPE((o), ESPM_TYPE_NOTIFY))

typedef enum
{
  ESPM_NOTIFY_LOW = 0,
  ESPM_NOTIFY_NORMAL,
  ESPM_NOTIFY_CRITICAL
} EspmNotifyUrgency;

typedef struct EspmNotifyPrivate EspmNotifyPrivate;

typedef struct
{
  GObject              parent;
  EspmNotifyPrivate   *priv;
} EspmNotify;

typedef struct
{
    GObjectClass          parent_class;
} EspmNotifyClass;

GType               espm_notify_get_type                      (void) G_GNUC_CONST;
EspmNotify         *espm_notify_new                           (void);
void                espm_notify_show_notification             (EspmNotify           *notify,
                                                               const gchar          *title,
                                                               const gchar          *text,
                                                               const gchar          *icon_name,
                                                               EspmNotifyUrgency     urgency);
NotifyNotification *espm_notify_new_notification              (EspmNotify           *notify,
                                                               const gchar          *title,
                                                               const gchar          *text,
                                                               const gchar          *icon_name,
                                                               EspmNotifyUrgency     urgency) G_GNUC_MALLOC;
void                espm_notify_add_action_to_notification    (EspmNotify           *notify,
                                                               NotifyNotification   *n,
                                                               const gchar          *id,
                                                               const gchar          *action_label,
                                                               NotifyActionCallback  callback,
                                                               gpointer              data);
void                espm_notify_present_notification          (EspmNotify            *notify,
                                                               NotifyNotification    *n);
void                espm_notify_critical                      (EspmNotify            *notify,
                                                               NotifyNotification    *n);
void                espm_notify_close_critical                (EspmNotify            *notify);
void                espm_notify_close_normal                  (EspmNotify            *notify);

G_END_DECLS

#endif /* __ESPM_NOTIFY_H */
