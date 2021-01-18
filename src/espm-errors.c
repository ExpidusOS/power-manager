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

#include <gio/gio.h>

#include "espm-errors.h"

GQuark
espm_get_error_quark (void)
{
  static volatile gsize espm_error_quark = 0;
  if (espm_error_quark == 0)
  {
    static const GDBusErrorEntry values[] =
    {
      { ESPM_ERROR_UNKNOWN, "com.expidus.PowerManager.Error.Unknown" },
      { ESPM_ERROR_PERMISSION_DENIED, "com.expidus.PowerManager.Error.PermissionDenied" },
      { ESPM_ERROR_NO_HARDWARE_SUPPORT, "com.expidus.PowerManager.Error.NoHardwareSupport" },
      { ESPM_ERROR_COOKIE_NOT_FOUND, "com.expidus.PowerManager.Error.CookieNotFound" },
      { ESPM_ERROR_INVALID_ARGUMENTS, "com.expidus.PowerManager.Error.InvalidArguments" },
      { ESPM_ERROR_SLEEP_FAILED, "com.expidus.PowerManager.Error.SleepFailed" },
    };

    g_dbus_error_register_error_domain ("espm-error-quark",
                                        &espm_error_quark,
                                        values,
                                        G_N_ELEMENTS (values));
  }

  return (GQuark) espm_error_quark;
}
