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

#ifndef __ESPM_POWER_H
#define __ESPM_POWER_H

#include <glib-object.h>
#include "espm-enum-glib.h"

G_BEGIN_DECLS

#define ESPM_TYPE_POWER        (espm_power_get_type () )
#define ESPM_POWER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ESPM_TYPE_POWER, EspmPower))
#define ESPM_IS_POWER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ESPM_TYPE_POWER))

typedef struct EspmPowerPrivate EspmPowerPrivate;

typedef struct
{
    GObject               parent;
    EspmPowerPrivate     *priv;
} EspmPower;

typedef struct
{
    GObjectClass   parent_class;
    void         (*on_battery_changed)           (EspmPower *power,
                                                  gboolean on_battery);
    void         (*low_battery_changed)          (EspmPower *power,
                                                  gboolean low_battery);
    void         (*lid_changed)                  (EspmPower *power,
                                                  gboolean lid_is_closed);
    void         (*waking_up)                    (EspmPower *power);
    void         (*sleeping)                     (EspmPower *power);
    void         (*ask_shutdown)                 (EspmPower *power);
    void         (*shutdown)                     (EspmPower *power);

} EspmPowerClass;

GType       espm_power_get_type                 (void) G_GNUC_CONST;
EspmPower  *espm_power_get                      (void);
void        espm_power_suspend                  (EspmPower *power,
                                                 gboolean force);
void        espm_power_hibernate                (EspmPower *power,
                                                 gboolean force);
gboolean    espm_power_has_battery              (EspmPower *power);
gboolean    espm_power_is_in_presentation_mode  (EspmPower *power);

G_END_DECLS

#endif /* __ESPM_POWER_H */
