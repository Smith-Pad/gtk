/* gtkatspiselection.c: AT-SPI Selection implementation
 *
 * Copyright 2020 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkatspiselectionprivate.h"

#include "a11y/atspi/atspi-selection.h"

#include "gtkatcontextprivate.h"
#include "gtkatspicontextprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkdebug.h"
#include "gtklistbox.h"
#include "gtkflowbox.h"
#include "gtkcombobox.h"
#include "gtkstackswitcher.h"

#include <gio/gio.h>

typedef struct {
  int n;
  GtkWidget *child;
} Counter;

static void
find_nth (GtkWidget *box,
          GtkWidget *child,
          gpointer   data)
{
  Counter *counter = data;

  if (counter->n == 0)
    counter->child = child;

  counter->n--;
}

static void
listbox_handle_method (GDBusConnection       *connection,
                       const gchar           *sender,
                       const gchar           *object_path,
                       const gchar           *interface_name,
                       const gchar           *method_name,
                       GVariant              *parameters,
                       GDBusMethodInvocation *invocation,
                       gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetSelectedChild") == 0)
    {
      Counter counter;
      int idx;

      g_variant_get (parameters, "(i)", &idx);

      counter.n = idx;
      counter.child = NULL;

      gtk_list_box_selected_foreach (GTK_LIST_BOX (widget), (GtkListBoxForeachFunc)find_nth, &counter);

      if (counter.child == NULL)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No selected child for %d", idx);
      else
        {
          GtkATContext *ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (counter.child));
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (ctx))));
        }
    }
  else if (g_strcmp0 (method_name, "SelectChild") == 0)
    {
      int idx;
      GtkListBoxRow *row;

      g_variant_get (parameters, "(i)", &idx);

      row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (widget), idx);
      if (!row)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        {
          gboolean ret;

          gtk_list_box_select_row (GTK_LIST_BOX (widget), row);
          ret = gtk_list_box_row_is_selected (row);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "DeselectChild") == 0)
    {
      int idx;
      GtkListBoxRow *row;

      g_variant_get (parameters, "(i)", &idx);

      row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (widget), idx);
      if (!row)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        {
          gboolean ret;

          gtk_list_box_unselect_row (GTK_LIST_BOX (widget), row);
          ret = !gtk_list_box_row_is_selected (row);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "DeselectSelectedChild") == 0)
    {
      Counter counter;
      int idx;

      g_variant_get (parameters, "(i)", &idx);

      counter.n = idx;
      counter.child = NULL;

      gtk_list_box_selected_foreach (GTK_LIST_BOX (widget), (GtkListBoxForeachFunc)find_nth, &counter);

      if (counter.child == NULL)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No selected child for %d", idx);
      else
        {
          gboolean ret;

          gtk_list_box_unselect_row (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (counter.child));
          ret = !gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (counter.child));
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "IsChildSelected") == 0)
    {
      int idx;
      GtkListBoxRow *row;

      g_variant_get (parameters, "(i)", &idx);

      row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (widget), idx);
      if (!row)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", gtk_list_box_row_is_selected (row)));
    }
  else if (g_strcmp0 (method_name, "SelectAll") == 0)
    {
      gtk_list_box_select_all (GTK_LIST_BOX (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "ClearSelection") == 0)
    {
      gtk_list_box_unselect_all (GTK_LIST_BOX (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
}

static void
count_selected (GtkWidget *box,
                GtkWidget *child,
                gpointer   data)
{
  *(int *)data += 1;
}

static GVariant *
listbox_get_property (GDBusConnection  *connection,
                      const gchar      *sender,
                      const gchar      *object_path,
                      const gchar      *interface_name,
                      const gchar      *property_name,
                      GError          **error,
                      gpointer          user_data)
{
  GtkATContext *self = GTK_AT_CONTEXT (user_data);
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "NSelectedChildren") == 0)
    {
      int count = 0;

      gtk_list_box_selected_foreach (GTK_LIST_BOX (widget), (GtkListBoxForeachFunc)count_selected, &count);

      return g_variant_new_int32 (count);
    }

  return NULL;
}

static const GDBusInterfaceVTable listbox_vtable = {
  listbox_handle_method,
  listbox_get_property,
  NULL
};


static void
flowbox_handle_method (GDBusConnection       *connection,
                       const gchar           *sender,
                       const gchar           *object_path,
                       const gchar           *interface_name,
                       const gchar           *method_name,
                       GVariant              *parameters,
                       GDBusMethodInvocation *invocation,
                       gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetSelectedChild") == 0)
    {
      Counter counter;
      int idx;

      g_variant_get (parameters, "(i)", &idx);

      counter.n = idx;
      counter.child = NULL;

      gtk_flow_box_selected_foreach (GTK_FLOW_BOX (widget), (GtkFlowBoxForeachFunc)find_nth, &counter);

      if (counter.child == NULL)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No selected child for %d", idx);
      else
        {
          GtkATContext *ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (counter.child));
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (ctx))));
        }
    }
  else if (g_strcmp0 (method_name, "SelectChild") == 0)
    {
      int idx;
      GtkFlowBoxChild *child;

      g_variant_get (parameters, "(i)", &idx);

      child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (widget), idx);
      if (!child)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        {
          gboolean ret;

          gtk_flow_box_select_child (GTK_FLOW_BOX (widget), child);
          ret = gtk_flow_box_child_is_selected (child);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "DeselectChild") == 0)
    {
      int idx;
      GtkFlowBoxChild *child;

      g_variant_get (parameters, "(i)", &idx);

      child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (widget), idx);
      if (!child)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        {
          gboolean ret;

          gtk_flow_box_unselect_child (GTK_FLOW_BOX (widget), child);
          ret = !gtk_flow_box_child_is_selected (child);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "DeselectSelectedChild") == 0)
    {
      Counter counter;
      int idx;

      g_variant_get (parameters, "(i)", &idx);

      counter.n = idx;
      counter.child = NULL;

      gtk_flow_box_selected_foreach (GTK_FLOW_BOX (widget), (GtkFlowBoxForeachFunc)find_nth, &counter);

      if (counter.child == NULL)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No selected child for %d", idx);
      else
        {
          gboolean ret;

          gtk_flow_box_unselect_child (GTK_FLOW_BOX (widget), GTK_FLOW_BOX_CHILD (counter.child));
          ret = !gtk_flow_box_child_is_selected (GTK_FLOW_BOX_CHILD (counter.child));
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
        }
    }
  else if (g_strcmp0 (method_name, "IsChildSelected") == 0)
    {
      int idx;
      GtkFlowBoxChild *child;

      g_variant_get (parameters, "(i)", &idx);

      child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (widget), idx);
      if (!child)
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "No child at position %d", idx);
      else
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", gtk_flow_box_child_is_selected (child)));
    }
  else if (g_strcmp0 (method_name, "SelectAll") == 0)
    {
      gtk_flow_box_select_all (GTK_FLOW_BOX (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "ClearSelection") == 0)
    {
      gtk_flow_box_unselect_all (GTK_FLOW_BOX (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
}

static GVariant *
flowbox_get_property (GDBusConnection  *connection,
                      const gchar      *sender,
                      const gchar      *object_path,
                      const gchar      *interface_name,
                      const gchar      *property_name,
                      GError          **error,
                      gpointer          user_data)
{
  GtkATContext *self = GTK_AT_CONTEXT (user_data);
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "NSelectedChildren") == 0)
    {
      int count = 0;

      gtk_flow_box_selected_foreach (GTK_FLOW_BOX (widget), (GtkFlowBoxForeachFunc)count_selected, &count);

      return g_variant_new_int32 (count);
    }

  return NULL;
}

static const GDBusInterfaceVTable flowbox_vtable = {
  flowbox_handle_method,
  flowbox_get_property,
  NULL
};


static void
combobox_handle_method (GDBusConnection       *connection,
                        const gchar           *sender,
                        const gchar           *object_path,
                        const gchar           *interface_name,
                        const gchar           *method_name,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "GetSelectedChild") == 0)
    {
      /* Need to figure out what to do here */
      g_dbus_method_invocation_return_error_literal (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    }
  else if (g_strcmp0 (method_name, "SelectChild") == 0)
    {
      int idx;

      g_variant_get (parameters, "(i)", &idx);
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), idx);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "DeselectChild") == 0)
    {
      int idx;

      g_variant_get (parameters, "(i)", &idx);

      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), -1);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "DeselectSelectedChild") == 0)
    {
      int idx;

      g_variant_get (parameters, "(i)", &idx);
      if (idx == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), -1);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", idx == 0));
    }
  else if (g_strcmp0 (method_name, "IsChildSelected") == 0)
    {
      int idx;
      gboolean active;

      g_variant_get (parameters, "(i)", &idx);
      active = idx = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", active));
    }
  else if (g_strcmp0 (method_name, "SelectAll") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
  else if (g_strcmp0 (method_name, "ClearSelection") == 0)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), -1);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
}

static GVariant *
combobox_get_property (GDBusConnection  *connection,
                       const gchar      *sender,
                       const gchar      *object_path,
                       const gchar      *interface_name,
                       const gchar      *property_name,
                       GError          **error,
                       gpointer          user_data)
{
  GtkATContext *self = GTK_AT_CONTEXT (user_data);
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "NSelectedChildren") == 0)
    {
      if (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
        return g_variant_new_int32 (1);
      else
        return g_variant_new_int32 (0);
    }

  return NULL;
}

static const GDBusInterfaceVTable combobox_vtable = {
  combobox_handle_method,
  combobox_get_property,
  NULL
};


static void
stackswitcher_handle_method (GDBusConnection       *connection,
                             const gchar           *sender,
                             const gchar           *object_path,
                             const gchar           *interface_name,
                             const gchar           *method_name,
                             GVariant              *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);
  GtkStack *stack = gtk_stack_switcher_get_stack (GTK_STACK_SWITCHER (widget));

  if (g_strcmp0 (method_name, "GetSelectedChild") == 0)
    {
      guint i, n;
      GtkSelectionModel *pages;
      GtkWidget *child;

      pages = gtk_stack_get_pages (stack);
      n = g_list_model_get_n_items (G_LIST_MODEL (pages));
      for (i = 0, child = gtk_widget_get_first_child (widget);
           i < n && child;
           i++, child = gtk_widget_get_next_sibling (child))
        {
          if (gtk_selection_model_is_selected (pages, i))
            break;
        }
      g_object_unref (pages);

      if (child == NULL)
        g_dbus_method_invocation_return_error_literal (invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_INVALID_ARGS,
                                                       "No selected child");
      else
        {
          GtkATContext *ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));
          g_dbus_method_invocation_return_value (invocation,
               g_variant_new ("(@(so))", gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (ctx))));
        }
    }
  else if (g_strcmp0 (method_name, "SelectChild") == 0)
    {
      int idx;
      GtkSelectionModel *pages;

      g_variant_get (parameters, "(i)", &idx);

      pages = gtk_stack_get_pages (stack);
      gtk_selection_model_select_item (pages, idx, TRUE);
      g_object_unref (pages);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
  else if (g_strcmp0 (method_name, "DeselectChild") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
  else if (g_strcmp0 (method_name, "DeselectSelectedChild") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
  else if (g_strcmp0 (method_name, "IsChildSelected") == 0)
    {
      int idx;
      GtkSelectionModel *pages;
      gboolean active;

      g_variant_get (parameters, "(i)", &idx);

      pages = gtk_stack_get_pages (stack);
      active = gtk_selection_model_is_selected (pages, idx);
      g_object_unref (pages);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", active));
    }
  else if (g_strcmp0 (method_name, "SelectAll") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
  else if (g_strcmp0 (method_name, "ClearSelection") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
}

static GVariant *
stackswitcher_get_property (GDBusConnection  *connection,
                            const gchar      *sender,
                            const gchar      *object_path,
                            const gchar      *interface_name,
                            const gchar      *property_name,
                            GError          **error,
                            gpointer          user_data)
{
  GtkATContext *self = GTK_AT_CONTEXT (user_data);
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "NSelectedChildren") == 0)
    {
      GtkStack *stack = gtk_stack_switcher_get_stack (GTK_STACK_SWITCHER (widget));

      if (stack == NULL || gtk_stack_get_visible_child (stack) == NULL)
        return g_variant_new_int32 (0);
      else
        return g_variant_new_int32 (1);
    }

  return NULL;
}
static const GDBusInterfaceVTable stackswitcher_vtable = {
  stackswitcher_handle_method,
  stackswitcher_get_property,
  NULL
};

const GDBusInterfaceVTable *
gtk_atspi_get_selection_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_LIST_BOX (accessible))
    return &listbox_vtable;
  else if (GTK_IS_FLOW_BOX (accessible))
    return &flowbox_vtable;
  else if (GTK_IS_COMBO_BOX (accessible))
    return &combobox_vtable;
  else if (GTK_IS_STACK_SWITCHER (accessible))
    return &stackswitcher_vtable;

  return NULL;
}

typedef struct {
  GtkAtspiSelectionCallback *changed;
  gpointer data;
} SelectionChanged;

void
gtk_atspi_connect_selection_signals (GtkAccessible *accessible,
                                     GtkAtspiSelectionCallback selection_changed,
                                     gpointer   data)
{
  if (GTK_IS_LIST_BOX (accessible))
    {
      SelectionChanged *changed;

      changed = g_new (SelectionChanged, 1);
      changed->changed = selection_changed;
      changed->data = data;

      g_object_set_data_full (G_OBJECT (accessible), "accessible-selection-data", changed, g_free);

      g_signal_connect_swapped (accessible, "selected-rows-changed", G_CALLBACK (selection_changed), data);
    }
  else if (GTK_IS_FLOW_BOX (accessible))
    {
      SelectionChanged *changed;

      changed = g_new (SelectionChanged, 1);
      changed->changed = selection_changed;
      changed->data = data;

      g_object_set_data_full (G_OBJECT (accessible), "accessible-selection-data", changed, g_free);

      g_signal_connect_swapped (accessible, "selected-children-changed", G_CALLBACK (selection_changed), data);
    }
  else if (GTK_IS_COMBO_BOX (accessible))
    {
      SelectionChanged *changed;

      changed = g_new (SelectionChanged, 1);
      changed->changed = selection_changed;
      changed->data = data;

      g_object_set_data_full (G_OBJECT (accessible), "accessible-selection-data", changed, g_free);

      g_signal_connect_swapped (accessible, "changed", G_CALLBACK (selection_changed), data);
    }
  else if (GTK_IS_STACK_SWITCHER (accessible))
    {
      SelectionChanged *changed;

      changed = g_new (SelectionChanged, 1);
      changed->changed = selection_changed;
      changed->data = data;

      g_object_set_data_full (G_OBJECT (accessible), "accessible-selection-data", changed, g_free);

      g_signal_connect_swapped (accessible, "notify::visible-child", G_CALLBACK (selection_changed), data);
    }
}

void
gtk_atspi_disconnect_selection_signals (GtkAccessible *accessible)
{
  if (GTK_IS_LIST_BOX (accessible) ||
      GTK_IS_FLOW_BOX (accessible) ||
      GTK_IS_COMBO_BOX (accessible) ||
      GTK_IS_STACK_SWITCHER (accessible))
    {
      SelectionChanged *changed;

      changed = g_object_get_data (G_OBJECT (accessible), "accessible-selection-data");

      g_signal_handlers_disconnect_by_func (accessible, changed->changed, changed->data);

      g_object_set_data (G_OBJECT (accessible), "accessible-selection-data", NULL);
    }
}