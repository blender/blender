/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_dbus_dynload
 *
 * Wrapper functions for `<dbus/dbus.h>`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DBUS_DYNLOAD_FN
DBUS_DYNLOAD_FN(dbus_error_init)
DBUS_DYNLOAD_FN(dbus_error_is_set)
DBUS_DYNLOAD_FN(dbus_error_free)
DBUS_DYNLOAD_FN(dbus_message_new_method_call)
DBUS_DYNLOAD_FN(dbus_message_append_args)
DBUS_DYNLOAD_FN(dbus_message_is_signal)
DBUS_DYNLOAD_FN(dbus_message_unref)
DBUS_DYNLOAD_FN(dbus_connection_send_with_reply_and_block)
DBUS_DYNLOAD_FN(dbus_connection_read_write_dispatch)
DBUS_DYNLOAD_FN(dbus_connection_get_dispatch_status)
DBUS_DYNLOAD_FN(dbus_connection_get_unix_fd)
DBUS_DYNLOAD_FN(dbus_connection_set_exit_on_disconnect)
DBUS_DYNLOAD_FN(dbus_connection_add_filter)
DBUS_DYNLOAD_FN(dbus_connection_flush)
DBUS_DYNLOAD_FN(dbus_connection_close)
DBUS_DYNLOAD_FN(dbus_connection_unref)
DBUS_DYNLOAD_FN(dbus_message_iter_init)
DBUS_DYNLOAD_FN(dbus_message_iter_next)
DBUS_DYNLOAD_FN(dbus_message_iter_get_arg_type)
DBUS_DYNLOAD_FN(dbus_message_iter_recurse)
DBUS_DYNLOAD_FN(dbus_message_iter_get_basic)
DBUS_DYNLOAD_FN(dbus_bus_get_private)
DBUS_DYNLOAD_FN(dbus_bus_add_match)
#else

/* Header guard. */
#  if !defined(__DBUS_DYNLOAD_H__) && !defined(DBUS_DYNLOAD_VALIDATE)
#    define __DBUS_DYNLOAD_H__

#    ifndef DBUS_DYNLOAD_VALIDATE
#      include <dbus/dbus.h>
extern struct DBusDynload dbus_dynload;
#    endif

/* Support validating declarations against the header. */
#    ifndef DBUS_DYNLOAD_VALIDATE
#      define DBUS_DYN_FN(sym) (*sym)
#    else
#      define DBUS_DYN_FN(sym) (sym)
#    endif

#    ifndef DBUS_DYNLOAD_VALIDATE
struct DBusDynload {
#    endif

  void DBUS_DYN_FN(dbus_error_init)(DBusError *error);
  dbus_bool_t DBUS_DYN_FN(dbus_error_is_set)(const DBusError *error);
  void DBUS_DYN_FN(dbus_error_free)(DBusError *error);
  DBusMessage *DBUS_DYN_FN(dbus_message_new_method_call)(const char *bus_name,
                                                         const char *path,
                                                         const char *iface,
                                                         const char *method);
  dbus_bool_t DBUS_DYN_FN(dbus_message_append_args)(DBusMessage *message, int first_arg_type, ...);
  dbus_bool_t DBUS_DYN_FN(dbus_message_is_signal)(DBusMessage *message,
                                                  const char *iface,
                                                  const char *signal_name);
  void DBUS_DYN_FN(dbus_message_unref)(DBusMessage *message);
  DBusMessage *DBUS_DYN_FN(dbus_connection_send_with_reply_and_block)(DBusConnection *connection,
                                                                      DBusMessage *message,
                                                                      int timeout_milliseconds,
                                                                      DBusError *error);
  dbus_bool_t DBUS_DYN_FN(dbus_connection_read_write_dispatch)(DBusConnection *connection,
                                                               int timeout_milliseconds);
  DBusDispatchStatus DBUS_DYN_FN(dbus_connection_get_dispatch_status)(DBusConnection *connection);
  dbus_bool_t DBUS_DYN_FN(dbus_connection_get_unix_fd)(DBusConnection *connection, int *fd);
  void DBUS_DYN_FN(dbus_connection_set_exit_on_disconnect)(DBusConnection *connection,
                                                           dbus_bool_t exit_on_disconnect);
  dbus_bool_t DBUS_DYN_FN(dbus_connection_add_filter)(DBusConnection *connection,
                                                      DBusHandleMessageFunction function,
                                                      void *user_data,
                                                      DBusFreeFunction free_data_function);
  void DBUS_DYN_FN(dbus_connection_flush)(DBusConnection *connection);
  void DBUS_DYN_FN(dbus_connection_close)(DBusConnection *connection);
  void DBUS_DYN_FN(dbus_connection_unref)(DBusConnection *connection);
  dbus_bool_t DBUS_DYN_FN(dbus_message_iter_init)(DBusMessage *message, DBusMessageIter *iter);
  dbus_bool_t DBUS_DYN_FN(dbus_message_iter_next)(DBusMessageIter *iter);
  int DBUS_DYN_FN(dbus_message_iter_get_arg_type)(DBusMessageIter *iter);
  void DBUS_DYN_FN(dbus_message_iter_recurse)(DBusMessageIter *iter, DBusMessageIter *sub);
  void DBUS_DYN_FN(dbus_message_iter_get_basic)(DBusMessageIter *iter, void *value);
  DBusConnection *DBUS_DYN_FN(dbus_bus_get_private)(DBusBusType type, DBusError *error);
  void DBUS_DYN_FN(dbus_bus_add_match)(DBusConnection *connection,
                                       const char *rule,
                                       DBusError *error);

#    ifndef DBUS_DYNLOAD_VALIDATE
};
#    endif
#    undef DBUS_DYN_FN

#    ifndef DBUS_DYNLOAD_VALIDATE
#      define dbus_error_init(...) (*dbus_dynload.dbus_error_init)(__VA_ARGS__)
#      define dbus_error_is_set(...) (*dbus_dynload.dbus_error_is_set)(__VA_ARGS__)
#      define dbus_error_free(...) (*dbus_dynload.dbus_error_free)(__VA_ARGS__)
#      define dbus_message_new_method_call(...) \
        (*dbus_dynload.dbus_message_new_method_call)(__VA_ARGS__)
#      define dbus_message_append_args(...) (*dbus_dynload.dbus_message_append_args)(__VA_ARGS__)
#      define dbus_message_is_signal(...) (*dbus_dynload.dbus_message_is_signal)(__VA_ARGS__)
#      define dbus_message_unref(...) (*dbus_dynload.dbus_message_unref)(__VA_ARGS__)
#      define dbus_connection_send_with_reply_and_block(...) \
        (*dbus_dynload.dbus_connection_send_with_reply_and_block)(__VA_ARGS__)
#      define dbus_connection_read_write_dispatch(...) \
        (*dbus_dynload.dbus_connection_read_write_dispatch)(__VA_ARGS__)
#      define dbus_connection_get_dispatch_status(...) \
        (*dbus_dynload.dbus_connection_get_dispatch_status)(__VA_ARGS__)
#      define dbus_connection_get_unix_fd(...) \
        (*dbus_dynload.dbus_connection_get_unix_fd)(__VA_ARGS__)
#      define dbus_connection_set_exit_on_disconnect(...) \
        (*dbus_dynload.dbus_connection_set_exit_on_disconnect)(__VA_ARGS__)
#      define dbus_connection_add_filter(...) \
        (*dbus_dynload.dbus_connection_add_filter)(__VA_ARGS__)
#      define dbus_connection_flush(...) (*dbus_dynload.dbus_connection_flush)(__VA_ARGS__)
#      define dbus_connection_close(...) (*dbus_dynload.dbus_connection_close)(__VA_ARGS__)
#      define dbus_connection_unref(...) (*dbus_dynload.dbus_connection_unref)(__VA_ARGS__)
#      define dbus_message_iter_init(...) (*dbus_dynload.dbus_message_iter_init)(__VA_ARGS__)
#      define dbus_message_iter_next(...) (*dbus_dynload.dbus_message_iter_next)(__VA_ARGS__)
#      define dbus_message_iter_get_arg_type(...) \
        (*dbus_dynload.dbus_message_iter_get_arg_type)(__VA_ARGS__)
#      define dbus_message_iter_recurse(...) (*dbus_dynload.dbus_message_iter_recurse)(__VA_ARGS__)
#      define dbus_message_iter_get_basic(...) \
        (*dbus_dynload.dbus_message_iter_get_basic)(__VA_ARGS__)
#      define dbus_bus_get_private(...) (*dbus_dynload.dbus_bus_get_private)(__VA_ARGS__)
#      define dbus_bus_add_match(...) (*dbus_dynload.dbus_bus_add_match)(__VA_ARGS__)
#    endif /* !DBUS_DYNLOAD_VALIDATE */
#  endif   /* !defined(__DBUS_DYNLOAD_H__) && !defined(DBUS_DYNLOAD_VALIDATE) */
#endif     /* !defined(DBUS_DYNLOAD_FN) */

#ifdef __cplusplus
}
#endif
