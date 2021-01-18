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

#ifndef __ESPM_ERRORS_H
#define __ESPM_ERRORS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ESPM_ERROR      (espm_get_error_quark ())


typedef enum
{
  ESPM_ERROR_UNKNOWN = 0,
  ESPM_ERROR_PERMISSION_DENIED,
  ESPM_ERROR_NO_HARDWARE_SUPPORT,
  ESPM_ERROR_COOKIE_NOT_FOUND,
  ESPM_ERROR_INVALID_ARGUMENTS,
  ESPM_ERROR_SLEEP_FAILED

} EspmError;

GQuark  espm_get_error_quark (void);


G_END_DECLS

#endif /*__ESPM_ERRORS_H */
