/*
 * * Copyright (C) 2014 Eric Koegel <eric@expidus.org>
 * * Copyright (C) 2019 Kacper Piwiński
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
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#ifdef EXPIDUS_PLUGIN
#include <libexpidus1panel/libexpidus1panel.h>
#include <libexpidus1ui/libexpidus1ui.h>
#include <libexpidus1util/libexpidus1util.h>
#include <esconf/esconf.h>
#endif

#include "../power-manager-button.h"
#include "common/espm-config.h"

/* plugin structure */
typedef struct
{
  ExpidusPanelPlugin *plugin;

  /* panel widgets */
  GtkWidget       *ebox;
  GtkWidget       *power_manager_button;
}
PowerManagerPlugin;

enum {
  COLUMN_INT,
  COLUMN_STRING,
  N_COLUMNS
};

/* prototypes */
static void power_manager_plugin_construct (ExpidusPanelPlugin *plugin);
/* register the plugin */
EXPIDUS_PANEL_PLUGIN_REGISTER (power_manager_plugin_construct);

static void
power_manager_plugin_configure_response (GtkWidget    *dialog,
                           gint          response,
                           PowerManagerPlugin *power_manager_plugin)
{
  gboolean result;

  if (response == GTK_RESPONSE_HELP)
  {
    result = g_spawn_command_line_async ("exo-open --launch WebBrowser " "http://docs.expidus.org/expidus/expidus1-power-manager/1.6/start", NULL);

    if (G_UNLIKELY (result == FALSE))
      g_warning (_("Unable to open the following url: %s"), "http://docs.expidus.org/expidus/expidus1-power-manager/1.6/start");
  }
  else
  {
    g_object_set_data (G_OBJECT (power_manager_plugin->plugin), "dialog", NULL);
    expidus_panel_plugin_unblock_menu (power_manager_plugin->plugin);
    gtk_widget_destroy (dialog);
  }
}

/* Update combo if property in channel changes */
static void
power_manager_plugin_panel_label_changed (EsconfChannel *channel,
                                          const gchar *property,
                                          const GValue *value,
                                          gpointer user_data)
{
  GtkWidget *combo = user_data;
  GtkListStore *list_store;
  GtkTreeIter iter;
  int show_panel_label, current_setting;

  list_store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo)));
  current_setting = g_value_get_int (value);
  /* If the value set in esconf is invalid, treat it like 0 aka "None" */
  if (current_setting < 0 ||
      current_setting > 3)
      current_setting = 0;

  for (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
        gtk_list_store_iter_is_valid (list_store, &iter);
        gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter))
  {
    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter, 0, &show_panel_label, -1);
    if (show_panel_label == current_setting)
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo),
                                     &iter);
  }
}

/* Update esconf property if combobox selection is changed */
static void
power_manager_plugin_combo_changed (GtkComboBox *combo,
                                    gpointer user_data)
{
  EsconfChannel *channel = user_data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  int show_panel_label;

  if (!gtk_combo_box_get_active_iter (combo, &iter))
    return;

  model = gtk_combo_box_get_model (combo);

  gtk_tree_model_get (model, &iter, 0, &show_panel_label, -1);
  esconf_channel_set_int (channel, ESPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL, show_panel_label);
}

static void
power_manager_plugin_configure (ExpidusPanelPlugin      *plugin,
                                PowerManagerPlugin   *power_manager_plugin)
{
  GtkWidget *dialog;
  GtkWidget *grid, *combo, *label, *gtkswitch;
  gint show_panel_label;
  EsconfChannel   *channel;
  GtkListStore *list_store;
  GtkTreeIter iter, active_iter;
  GtkCellRenderer *cell;
  gint i;
  gchar *options[] = { _("None"), _("Percentage"), _("Remaining time"), _("Percentage and remaining time") };

  channel = esconf_channel_get (ESPM_CHANNEL);

  /* block the plugin menu */
  expidus_panel_plugin_block_menu (plugin);

  /* create the dialog */
  dialog = expidus_titled_dialog_new_with_mixed_buttons (_("Power Manager Plugin Settings"),
                                                      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "help-browser", _("_Help"), GTK_RESPONSE_HELP,
                                                      "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
                                                      NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "com.expidus.powermanager");
  gtk_widget_show (dialog);

  /* Set up the main grid for all settings */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_margin_start (grid, 12);
  gtk_widget_set_margin_end (grid, 12);
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_container_add_with_properties (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                                     grid, "expand", TRUE, "fill", TRUE, NULL);

  /* show-panel-label setting */
  label = gtk_label_new (_("Show label:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label), 0, 0, 1, 1);
  show_panel_label = esconf_channel_get_int (channel, ESPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL, -1);

  list_store = gtk_list_store_new (N_COLUMNS,
                                   G_TYPE_INT,
                                   G_TYPE_STRING);

  for (i = 0; i < 4; i++)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        COLUMN_INT, i,
                        COLUMN_STRING, options[i],
                        -1);
    if (i == show_panel_label)
      active_iter = iter;
  }
  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE );
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", COLUMN_STRING, NULL);
  gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), COLUMN_STRING);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &active_iter);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (combo), 1, 0, 1, 1);
  g_signal_connect (G_OBJECT (combo), "changed",
                    G_CALLBACK (power_manager_plugin_combo_changed),
                    channel);
  g_signal_connect (G_OBJECT (channel), "property-changed::" ESPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL,
                    G_CALLBACK (power_manager_plugin_panel_label_changed),
                    combo);

  label = gtk_label_new (_("Show 'Presentation mode' indicator:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label), 0, 1, 1, 1);

  gtkswitch = gtk_switch_new ();
  gtk_widget_set_halign (gtkswitch, GTK_ALIGN_END);
  gtk_widget_set_valign (gtkswitch, GTK_ALIGN_CENTER);

  esconf_g_property_bind (channel, ESPM_PROPERTIES_PREFIX SHOW_PRESENTATION_INDICATOR, G_TYPE_BOOLEAN,
                          G_OBJECT (gtkswitch), "active");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (gtkswitch), 1, 1, 1, 1);

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK(power_manager_plugin_configure_response), power_manager_plugin);
  gtk_widget_show_all (grid);
}


static PowerManagerPlugin *
power_manager_plugin_new (ExpidusPanelPlugin *plugin)
{
  PowerManagerPlugin *power_manager_plugin;
  EsconfChannel *channel;

  /* allocate memory for the plugin structure */
  power_manager_plugin = g_slice_new0 (PowerManagerPlugin);

  /* pointer to plugin */
  power_manager_plugin->plugin = plugin;

  /* create some panel ebox */
  power_manager_plugin->ebox = gtk_event_box_new ();
  gtk_widget_show (power_manager_plugin->ebox);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX(power_manager_plugin->ebox), FALSE);

  power_manager_plugin->power_manager_button = power_manager_button_new (plugin);
  gtk_container_add (GTK_CONTAINER (power_manager_plugin->ebox), power_manager_plugin->power_manager_button);
  power_manager_button_show (POWER_MANAGER_BUTTON (power_manager_plugin->power_manager_button));

  /* disable the systray item when the plugin is started, allowing the user to
  later manually enable it, e.g. for testing purposes. */
  channel = esconf_channel_get (ESPM_CHANNEL);
  if (esconf_channel_get_bool (channel, "/expidus1-power-manager/show-tray-icon", TRUE))
    g_warning ("Expidus1-power-manager: The panel plugin is present, so the tray icon gets disabled.");
  esconf_channel_set_bool (channel, "/expidus1-power-manager/show-tray-icon", FALSE);

  return power_manager_plugin;
}


static void
power_manager_plugin_construct (ExpidusPanelPlugin *plugin)
{
  PowerManagerPlugin *power_manager_plugin;

  expidus_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* create the plugin */
  power_manager_plugin = power_manager_plugin_new (plugin);

  /* show the configure menu item and connect signal */
  expidus_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (power_manager_plugin_configure), power_manager_plugin);

  /* add the ebox to the panel */
  gtk_container_add (GTK_CONTAINER (plugin), power_manager_plugin->ebox);
}
