/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#pragma once
#include <dbus/dbus.h>
#include <string>

static DBusMessage *get_setting_sync(DBusConnection *const connection,
                                     const char *key,
                                     const char *value)
{
  DBusError error;
  dbus_bool_t success;
  DBusMessage *message;
  DBusMessage *reply;

  dbus_error_init(&error);

  message = dbus_message_new_method_call("org.freedesktop.portal.Desktop",
                                         "/org/freedesktop/portal/desktop",
                                         "org.freedesktop.portal.Settings",
                                         "Read");

  success = dbus_message_append_args(
      message, DBUS_TYPE_STRING, &key, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);

  if (!success) {
    return NULL;
  }

  reply = dbus_connection_send_with_reply_and_block(
      connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);

  dbus_message_unref(message);

  if (dbus_error_is_set(&error)) {
    return NULL;
  }

  return reply;
}

static bool parse_type(DBusMessage *const reply, const int type, void *value)
{
  DBusMessageIter iter[3];

  dbus_message_iter_init(reply, &iter[0]);
  if (dbus_message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT) {
    return false;
  }

  dbus_message_iter_recurse(&iter[0], &iter[1]);
  if (dbus_message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT) {
    return false;
  }

  dbus_message_iter_recurse(&iter[1], &iter[2]);
  if (dbus_message_iter_get_arg_type(&iter[2]) != type) {
    return false;
  }

  dbus_message_iter_get_basic(&iter[2], value);

  return true;
}

static bool get_cursor_settings(std::string &theme, int &size)
{
  static const char name[] = "org.gnome.desktop.interface";
  static const char key_theme[] = "cursor-theme";
  static const char key_size[] = "cursor-size";

  DBusError error;
  DBusConnection *connection;
  DBusMessage *reply;
  const char *value_theme = NULL;

  dbus_error_init(&error);

  connection = dbus_bus_get(DBUS_BUS_SESSION, &error);

  if (dbus_error_is_set(&error)) {
    return false;
  }

  reply = get_setting_sync(connection, name, key_theme);
  if (!reply) {
    return false;
  }

  if (!parse_type(reply, DBUS_TYPE_STRING, &value_theme)) {
    dbus_message_unref(reply);
    return false;
  }

  theme = std::string(value_theme);

  dbus_message_unref(reply);

  reply = get_setting_sync(connection, name, key_size);
  if (!reply) {
    return false;
  }

  if (!parse_type(reply, DBUS_TYPE_INT32, &size)) {
    dbus_message_unref(reply);
    return false;
  }

  dbus_message_unref(reply);

  return true;
}
