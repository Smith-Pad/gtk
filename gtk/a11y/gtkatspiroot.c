/* gtkatspiroot.c: AT-SPI root object
 *
 * Copyright 2020  GNOME Foundation
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

#include "gtkatspirootprivate.h"

#include "gtkdebug.h"
#include "gtkwindow.h"

#include "a11y/atspi/atspi-accessible.h"
#include "a11y/atspi/atspi-application.h"

#include <locale.h>

#include <gio/gio.h>

#define ATSPI_VERSION   "2.1"
#define ATSPI_ROOT_PATH "/org/a11y/atspi/accessible/root"

struct _GtkAtSpiRoot
{
  GObject parent_instance;

  char *bus_address;
  GDBusConnection *connection;

  const char *root_path;

  const char *toolkit_name;
  const char *version;
  const char *atspi_version;

  char *desktop_name;
  char *desktop_path;

  gint32 application_id;
};

enum
{
  PROP_BUS_ADDRESS = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiRoot, gtk_at_spi_root, G_TYPE_OBJECT)

static void
gtk_at_spi_root_finalize (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  g_free (self->bus_address);
  g_free (self->desktop_name);
  g_free (self->desktop_path);

  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->dispose (gobject);
}

static void
gtk_at_spi_root_dispose (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->dispose (gobject);
}

static void
gtk_at_spi_root_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      self->bus_address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_root_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      g_value_set_string (value, self->bus_address);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}


static void
handle_application_method (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           const gchar           *interface_name,
                           const gchar           *method_name,
                           GVariant              *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
}

static GVariant *
handle_application_get_property (GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *property_name,
                                 GError               **error,
                                 gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;
  GVariant *res = NULL;

  if (g_strcmp0 (property_name, "Id") == 0)
    res = g_variant_new_int32 (self->application_id);
  else if (g_strcmp0 (property_name, "ToolkitName") == 0)
    res = g_variant_new_string (self->toolkit_name);
  else if (g_strcmp0 (property_name, "Version") == 0)
    res = g_variant_new_string (self->version);
  else if (g_strcmp0 (property_name, "AtspiVersion") == 0)
    res = g_variant_new_string (self->atspi_version);
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static gboolean
handle_application_set_property (GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *property_name,
                                 GVariant              *value,
                                 GError               **error,
                                 gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;

  if (g_strcmp0 (property_name, "Id") == 0)
    {
      g_variant_get (value, "i", &(self->application_id));
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Invalid property '%s'", property_name);
      return FALSE;
    }

  return TRUE;
}

static void
handle_accessible_method (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  g_printerr ("[Accessible] Method '%s' on interface '%s' for object '%s' from '%s'\n",
              method_name, interface_name, object_path, sender);

}

static GVariant *
handle_accessible_get_property (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *property_name,
                                GError               **error,
                                gpointer               user_data)
{
  GtkAtSpiRoot *self = user_data;
  GVariant *res = NULL;

  if (g_strcmp0 (property_name, "Name") == 0)
    res = g_variant_new_string (g_get_prgname ());
  else if (g_strcmp0 (property_name, "Description") == 0)
    res = g_variant_new_string (g_get_application_name ());
  else if (g_strcmp0 (property_name, "Locale") == 0)
    res = g_variant_new_string (setlocale (LC_MESSAGES, NULL));
  else if (g_strcmp0 (property_name, "AccessibleId") == 0)
    res = g_variant_new_string ("");
  else if (g_strcmp0 (property_name, "Parent") == 0)
    res = g_variant_new ("(so)", self->desktop_name, self->desktop_path);
  else if (g_strcmp0 (property_name, "ChildCount") == 0)
    {
      int n_children = 0;

      if (g_strcmp0 (object_path, ATSPI_ROOT_PATH) == 0)
        {
          GList *windows = gtk_window_list_toplevels ();

          /* We are only interested in the visible top levels */
          for (GList *l = windows; l != NULL; l = l->next)
            {
              if (gtk_widget_is_visible (l->data))
                n_children += 1;
            }

          g_list_free (windows);
        }

      res = g_variant_new_int32 (n_children);
    }
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable root_application_vtable = {
  handle_application_method,
  handle_application_get_property,
  handle_application_set_property,
};

static const GDBusInterfaceVTable root_accessible_vtable = {
  handle_accessible_method,
  handle_accessible_get_property,
  NULL,
};

static void
on_registration_reply (GObject      *gobject,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GtkAtSpiRoot *self = user_data;

  GError *error = NULL;
  GVariant *reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (gobject), result, &error);

  if (error != NULL)
    {
      g_critical ("Unable to register the application: %s", error->message);
      g_error_free (error);
      return;
    }

  if (reply != NULL)
    {
      g_variant_get (reply, "((so))",
                     &self->desktop_name,
                     &self->desktop_path);
      g_variant_unref (reply);

      GTK_NOTE (A11Y, g_message ("Connected to the a11y registry at (%s, %s)",
                                 self->desktop_name,
                                 self->desktop_path));
    }
}

static void
gtk_at_spi_root_register (GtkAtSpiRoot *self)
{
  /* Register the root element; every application has a single root, so we only
   * need to do this once.
   *
   * The root element is used to advertise our existence on the accessibility
   * bus, and it's the entry point to the accessible objects tree.
   *
   * The announcement is split into two phases:
   *
   *  1. we register the org.a11y.atspi.Application and org.a11y.atspi.Accessible
   *     interfaces at the well-known object path
   *  2. we invoke the org.a11y.atspi.Socket.Embed method with the connection's
   *     unique name and the object path
   *  3. the ATSPI registry daemon will set the org.a11y.atspi.Application.Id
   *     property on the given object path
   *  4. the registration concludes when the Embed method returns us the desktop
   *     name and object path
   */
  self->toolkit_name = "GTK";
  self->version = PACKAGE_VERSION;
  self->atspi_version = ATSPI_VERSION;
  self->root_path = ATSPI_ROOT_PATH;

  g_dbus_connection_register_object (self->connection,
                                     self->root_path,
                                     atspi_application_interface_info (),
                                     &root_application_vtable,
                                     self,
                                     NULL,
                                     NULL);
  g_dbus_connection_register_object (self->connection,
                                     self->root_path,
                                     atspi_accessible_interface_info (),
                                     &root_accessible_vtable,
                                     self,
                                     NULL,
                                     NULL);

  GTK_NOTE (A11Y, g_message ("Registering (%s, %s) on the a11y bus",
                             g_dbus_connection_get_unique_name (self->connection),
                             self->root_path));

  g_dbus_connection_call (self->connection,
                          "org.a11y.atspi.Registry",
                          ATSPI_ROOT_PATH,
                          "org.a11y.atspi.Socket",
                          "Embed",
                          g_variant_new ("((so))",
                            g_dbus_connection_get_unique_name (self->connection),
                            self->root_path
                          ),
                          G_VARIANT_TYPE ("((so))"),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL,
                          on_registration_reply,
                          self);
}

static void
gtk_at_spi_root_constructed (GObject *gobject)
{
  GtkAtSpiRoot *self = GTK_AT_SPI_ROOT (gobject);

  GError *error = NULL;

  /* The accessibility bus is a fully managed bus */
  self->connection =
    g_dbus_connection_new_for_address_sync (self->bus_address,
                                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                            NULL, NULL,
                                            &error);

  if (error != NULL)
    {
      g_critical ("Unable to connect to the accessibility bus at '%s': %s",
                  self->bus_address,
                  error->message);
      g_error_free (error);
      goto out;
    }

  gtk_at_spi_root_register (self);

out:
  G_OBJECT_CLASS (gtk_at_spi_root_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_root_class_init (GtkAtSpiRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_root_constructed;
  gobject_class->set_property = gtk_at_spi_root_set_property;
  gobject_class->get_property = gtk_at_spi_root_get_property;
  gobject_class->dispose = gtk_at_spi_root_dispose;
  gobject_class->finalize = gtk_at_spi_root_finalize;

  obj_props[PROP_BUS_ADDRESS] =
    g_param_spec_string ("bus-address", NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_root_init (GtkAtSpiRoot *self)
{
}

GtkAtSpiRoot *
gtk_at_spi_root_new (const char *bus_address)
{
  g_return_val_if_fail (bus_address != NULL, NULL);

  return g_object_new (GTK_TYPE_AT_SPI_ROOT,
                       "bus-address", bus_address,
                       NULL);
}

GDBusConnection *
gtk_at_spi_root_get_connection (GtkAtSpiRoot *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_ROOT (self), NULL);

  return self->connection;
}