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


/*
 * Based on code from gpm-button (gnome power manager)
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 *
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

#include <X11/X.h>
#include <X11/XF86keysym.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <glib.h>

#include <libexpidus1util/libexpidus1util.h>

#include "espm-button.h"
#include "espm-enum.h"
#include "espm-enum-types.h"
#include "espm-debug.h"

static void espm_button_finalize   (GObject *object);

static struct
{
  EspmButtonKey    key;
  guint            key_code;
} espm_key_map [NUMBER_OF_BUTTONS] = { {0, 0}, };

struct EspmButtonPrivate
{
  GdkScreen   *screen;
  GdkWindow   *window;
  guint16      mapped_buttons;
};

enum
{
  BUTTON_PRESSED,
  LAST_SIGNAL
};

#define DUPLICATE_SHUTDOWN_TIMEOUT 4.0f

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EspmButton, espm_button, G_TYPE_OBJECT)

static guint
espm_button_get_key (unsigned int keycode)
{
  EspmButtonKey key = BUTTON_UNKNOWN;
  guint i;

  for ( i = 0; i < G_N_ELEMENTS (espm_key_map); i++)
  {
    if ( espm_key_map [i].key_code == keycode )
      key = espm_key_map [i].key;
  }

  return key;
}

static GdkFilterReturn
espm_button_filter_x_events (GdkXEvent *xevent, GdkEvent *ev, gpointer data)
{
  EspmButtonKey key;
  EspmButton *button;

  XEvent *xev = (XEvent *) xevent;

  if ( xev->type != KeyPress )
    return GDK_FILTER_CONTINUE;

  key = espm_button_get_key (xev->xkey.keycode);

  if ( key != BUTTON_UNKNOWN )
  {
    button = (EspmButton *) data;

    ESPM_DEBUG_ENUM (key, ESPM_TYPE_BUTTON_KEY, "Key press");

    g_signal_emit (G_OBJECT(button), signals[BUTTON_PRESSED], 0, key);
    return GDK_FILTER_REMOVE;
  }

  return GDK_FILTER_CONTINUE;
}

static gboolean
espm_button_grab_keystring (EspmButton *button, guint keycode)
{
  Display *display;
  GdkDisplay *gdisplay;
  guint ret;
  guint modmask = AnyModifier;

  display = gdk_x11_get_default_xdisplay ();
  gdisplay = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (gdisplay);

  ret = XGrabKey (display, keycode, modmask,
                  GDK_WINDOW_XID (button->priv->window), True,
                  GrabModeAsync, GrabModeAsync);

  if ( ret == BadAccess )
  {
    g_warning ("Failed to grab modmask=%u, keycode=%li", modmask, (long int) keycode);
    return FALSE;
  }

  ret = XGrabKey (display, keycode, LockMask | modmask,
                  GDK_WINDOW_XID (button->priv->window), True,
                  GrabModeAsync, GrabModeAsync);

  if (ret == BadAccess)
  {
    g_warning ("Failed to grab modmask=%u, keycode=%li", LockMask | modmask, (long int) keycode);
    return FALSE;
  }

  gdk_display_flush (gdisplay);
  gdk_x11_display_error_trap_pop_ignored (gdisplay);

  return TRUE;
}


static gboolean
espm_button_xevent_key (EspmButton *button, guint keysym , EspmButtonKey key)
{
  guint keycode = XKeysymToKeycode (gdk_x11_get_default_xdisplay(), keysym);

  if ( keycode == 0 )
  {
    g_warning ("could not map keysym %x to keycode\n", keysym);
    return FALSE;
  }

  if ( !espm_button_grab_keystring(button, keycode))
  {
    g_warning ("Failed to grab %i\n", keycode);
    return FALSE;
  }

  ESPM_DEBUG_ENUM (key, ESPM_TYPE_BUTTON_KEY, "Grabbed key %li ", (long int) keycode);

  espm_key_map [key].key_code = keycode;
  espm_key_map [key].key = key;

  return TRUE;
}

static void
espm_button_setup (EspmButton *button)
{
  button->priv->screen = gdk_screen_get_default ();
  button->priv->window = gdk_screen_get_root_window (button->priv->screen);

  if ( espm_button_xevent_key (button, XF86XK_PowerOff, BUTTON_POWER_OFF) )
    button->priv->mapped_buttons |= POWER_KEY;

#ifdef HAVE_XF86XK_HIBERNATE
  if ( espm_button_xevent_key (button, XF86XK_Hibernate, BUTTON_HIBERNATE) )
    button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif

#ifdef HAVE_XF86XK_SUSPEND
  if ( espm_button_xevent_key (button, XF86XK_Suspend, BUTTON_HIBERNATE) )
    button->priv->mapped_buttons |= HIBERNATE_KEY;
#endif

  if ( espm_button_xevent_key (button, XF86XK_Sleep, BUTTON_SLEEP) )
    button->priv->mapped_buttons |= SLEEP_KEY;

  if ( espm_button_xevent_key (button, XF86XK_MonBrightnessUp, BUTTON_MON_BRIGHTNESS_UP) )
    button->priv->mapped_buttons |= BRIGHTNESS_KEY_UP;

  if (espm_button_xevent_key (button, XF86XK_MonBrightnessDown, BUTTON_MON_BRIGHTNESS_DOWN) )
    button->priv->mapped_buttons |= BRIGHTNESS_KEY_DOWN;

  if (espm_button_xevent_key (button, XF86XK_Battery, BUTTON_BATTERY))
    button->priv->mapped_buttons |= BATTERY_KEY;

  if ( espm_button_xevent_key (button, XF86XK_KbdBrightnessUp, BUTTON_KBD_BRIGHTNESS_UP) )
    button->priv->mapped_buttons |= KBD_BRIGHTNESS_KEY_UP;

  if (espm_button_xevent_key (button, XF86XK_KbdBrightnessDown, BUTTON_KBD_BRIGHTNESS_DOWN) )
    button->priv->mapped_buttons |= KBD_BRIGHTNESS_KEY_DOWN;

  gdk_window_add_filter (button->priv->window, espm_button_filter_x_events, button);
}

static void
espm_button_class_init(EspmButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  signals [BUTTON_PRESSED] =
      g_signal_new ("button-pressed",
                    ESPM_TYPE_BUTTON,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (EspmButtonClass, button_pressed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__ENUM,
                    G_TYPE_NONE, 1, ESPM_TYPE_BUTTON_KEY);

  object_class->finalize = espm_button_finalize;
}

static void
espm_button_init (EspmButton *button)
{
  button->priv = espm_button_get_instance_private (button);

  button->priv->mapped_buttons = 0;
  button->priv->screen = NULL;
  button->priv->window = NULL;

  espm_button_setup (button);
}

static void
espm_button_finalize (GObject *object)
{
  G_OBJECT_CLASS(espm_button_parent_class)->finalize(object);
}

EspmButton *
espm_button_new (void)
{
  static gpointer espm_button_object = NULL;

  if ( G_LIKELY (espm_button_object != NULL) )
  {
    g_object_ref (espm_button_object);
  }
  else
  {
    espm_button_object = g_object_new (ESPM_TYPE_BUTTON, NULL);
    g_object_add_weak_pointer (espm_button_object, &espm_button_object);
  }

    return ESPM_BUTTON (espm_button_object);
}

guint16
espm_button_get_mapped (EspmButton *button)
{
  g_return_val_if_fail (ESPM_IS_BUTTON (button), 0);

  return button->priv->mapped_buttons;
}
