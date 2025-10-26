/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-client.h>`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WAYLAND_DYNLOAD_FN
WAYLAND_DYNLOAD_FN(wl_display_connect)
WAYLAND_DYNLOAD_FN(wl_display_disconnect)
WAYLAND_DYNLOAD_FN(wl_display_dispatch)
WAYLAND_DYNLOAD_FN(wl_display_dispatch_pending)
WAYLAND_DYNLOAD_FN(wl_display_get_fd)
WAYLAND_DYNLOAD_FN(wl_display_get_protocol_error)
WAYLAND_DYNLOAD_FN(wl_display_prepare_read)
WAYLAND_DYNLOAD_FN(wl_display_read_events)
WAYLAND_DYNLOAD_FN(wl_display_cancel_read)
WAYLAND_DYNLOAD_FN(wl_display_roundtrip)
WAYLAND_DYNLOAD_FN(wl_display_flush)
WAYLAND_DYNLOAD_FN(wl_display_get_error)
WAYLAND_DYNLOAD_FN(wl_log_set_handler_client)
WAYLAND_DYNLOAD_FN(wl_proxy_add_listener)
WAYLAND_DYNLOAD_FN(wl_proxy_destroy)
WAYLAND_DYNLOAD_FN(wl_proxy_marshal_flags)
WAYLAND_DYNLOAD_FN(wl_proxy_marshal_array_flags)
WAYLAND_DYNLOAD_FN(wl_proxy_set_user_data)
WAYLAND_DYNLOAD_FN(wl_proxy_get_user_data)
WAYLAND_DYNLOAD_FN(wl_proxy_get_version)
WAYLAND_DYNLOAD_FN(wl_proxy_get_tag)
WAYLAND_DYNLOAD_FN(wl_proxy_set_tag)
#elif defined(WAYLAND_DYNLOAD_IFACE)
WAYLAND_DYNLOAD_IFACE(wl_buffer_interface)
WAYLAND_DYNLOAD_IFACE(wl_compositor_interface)
WAYLAND_DYNLOAD_IFACE(wl_data_device_interface)
WAYLAND_DYNLOAD_IFACE(wl_data_device_manager_interface)
WAYLAND_DYNLOAD_IFACE(wl_data_source_interface)
WAYLAND_DYNLOAD_IFACE(wl_keyboard_interface)
WAYLAND_DYNLOAD_IFACE(wl_output_interface)
WAYLAND_DYNLOAD_IFACE(wl_pointer_interface)
WAYLAND_DYNLOAD_IFACE(wl_region_interface)
WAYLAND_DYNLOAD_IFACE(wl_registry_interface)
WAYLAND_DYNLOAD_IFACE(wl_seat_interface)
WAYLAND_DYNLOAD_IFACE(wl_shm_interface)
WAYLAND_DYNLOAD_IFACE(wl_shm_pool_interface)
WAYLAND_DYNLOAD_IFACE(wl_surface_interface)
WAYLAND_DYNLOAD_IFACE(wl_touch_interface)
#else

/* Header guard. */
#  if !defined(__WAYLAND_DYNLOAD_CLIENT_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE)
#    define __WAYLAND_DYNLOAD_CLIENT_H__

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      include <wayland-client-core.h>
extern struct WaylandDynload_Client wayland_dynload_client;
#    endif

/* Support validating declarations against the header. */
#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define WL_DYN_FN(a) (*a)
#    else
#      define WL_DYN_FN(a) (a)
#    endif

#    ifndef WAYLAND_DYNLOAD_VALIDATE
struct WaylandDynload_Client {
#    endif
  struct wl_display *WL_DYN_FN(wl_display_connect)(const char *name);
  void WL_DYN_FN(wl_display_disconnect)(struct wl_display *display);
  int WL_DYN_FN(wl_display_dispatch)(struct wl_display *display);
  int WL_DYN_FN(wl_display_roundtrip)(struct wl_display *display);
  int WL_DYN_FN(wl_display_dispatch_pending)(struct wl_display *display);
  int WL_DYN_FN(wl_display_get_fd)(struct wl_display *display);
  uint32_t WL_DYN_FN(wl_display_get_protocol_error)(struct wl_display *display,
                                                    const struct wl_interface **interface,
                                                    uint32_t *id);
  int WL_DYN_FN(wl_display_prepare_read)(struct wl_display *display);
  int WL_DYN_FN(wl_display_read_events)(struct wl_display *display);
  void WL_DYN_FN(wl_display_cancel_read)(struct wl_display *display);
  int WL_DYN_FN(wl_display_flush)(struct wl_display *display);
  int WL_DYN_FN(wl_display_get_error)(struct wl_display *display);
  void WL_DYN_FN(wl_log_set_handler_client)(wl_log_func_t);
  int WL_DYN_FN(wl_proxy_add_listener)(struct wl_proxy *proxy,
                                       void (**implementation)(void),
                                       void *data);
  void WL_DYN_FN(wl_proxy_destroy)(struct wl_proxy *proxy);
  struct wl_proxy *WL_DYN_FN(wl_proxy_marshal_flags)(struct wl_proxy *proxy,
                                                     uint32_t opcode,
                                                     const struct wl_interface *interface,
                                                     uint32_t version,
                                                     uint32_t flags,
                                                     ...);
  struct wl_proxy *WL_DYN_FN(wl_proxy_marshal_array_flags)(struct wl_proxy *proxy,
                                                           uint32_t opcode,
                                                           const struct wl_interface *interface,
                                                           uint32_t version,
                                                           uint32_t flags,
                                                           union wl_argument *args);
  void WL_DYN_FN(wl_proxy_set_user_data)(struct wl_proxy *proxy, void *user_data);
  void *WL_DYN_FN(wl_proxy_get_user_data)(struct wl_proxy *proxy);
  uint32_t WL_DYN_FN(wl_proxy_get_version)(struct wl_proxy *proxy);
  const char *const *WL_DYN_FN(wl_proxy_get_tag)(struct wl_proxy *proxy);
  void WL_DYN_FN(wl_proxy_set_tag)(struct wl_proxy *proxy, const char *const *tag);

#    ifndef WAYLAND_DYNLOAD_VALIDATE
};
#    endif
#    undef WL_DYN_FN

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define wl_display_connect(...) (*wayland_dynload_client.wl_display_connect)(__VA_ARGS__)
#      define wl_display_disconnect(...) \
        (*wayland_dynload_client.wl_display_disconnect)(__VA_ARGS__)
#      define wl_display_dispatch(...) (*wayland_dynload_client.wl_display_dispatch)(__VA_ARGS__)
#      define wl_display_dispatch_pending(...) \
        (*wayland_dynload_client.wl_display_dispatch_pending)(__VA_ARGS__)
#      define wl_display_get_fd(...) (*wayland_dynload_client.wl_display_get_fd)(__VA_ARGS__)
#      define wl_display_get_protocol_error(...) \
        (*wayland_dynload_client.wl_display_get_protocol_error)(__VA_ARGS__)
#      define wl_display_prepare_read(...) \
        (*wayland_dynload_client.wl_display_prepare_read)(__VA_ARGS__)
#      define wl_display_read_events(...) \
        (*wayland_dynload_client.wl_display_read_events)(__VA_ARGS__)
#      define wl_display_cancel_read(...) \
        (*wayland_dynload_client.wl_display_cancel_read)(__VA_ARGS__)
#      define wl_display_roundtrip(...) (*wayland_dynload_client.wl_display_roundtrip)(__VA_ARGS__)
#      define wl_display_flush(...) (*wayland_dynload_client.wl_display_flush)(__VA_ARGS__)
#      define wl_display_get_error(...) (*wayland_dynload_client.wl_display_get_error)(__VA_ARGS__)
#      define wl_log_set_handler_client(...) \
        (*wayland_dynload_client.wl_log_set_handler_client)(__VA_ARGS__)
#      define wl_proxy_add_listener(...) \
        (*wayland_dynload_client.wl_proxy_add_listener)(__VA_ARGS__)
#      define wl_proxy_destroy(...) (*wayland_dynload_client.wl_proxy_destroy)(__VA_ARGS__)
#      define wl_proxy_marshal_flags(...) \
        (*wayland_dynload_client.wl_proxy_marshal_flags)(__VA_ARGS__)
#      define wl_proxy_marshal_array_flags(...) \
        (*wayland_dynload_client.wl_proxy_marshal_array_flags)(__VA_ARGS__)
#      define wl_proxy_set_user_data(...) \
        (*wayland_dynload_client.wl_proxy_set_user_data)(__VA_ARGS__)
#      define wl_proxy_get_user_data(...) \
        (*wayland_dynload_client.wl_proxy_get_user_data)(__VA_ARGS__)
#      define wl_proxy_get_version(...) (*wayland_dynload_client.wl_proxy_get_version)(__VA_ARGS__)
#      define wl_proxy_get_tag(...) (*wayland_dynload_client.wl_proxy_get_tag)(__VA_ARGS__)
#      define wl_proxy_set_tag(...) (*wayland_dynload_client.wl_proxy_set_tag)(__VA_ARGS__)

#    endif /* !WAYLAND_DYNLOAD_VALIDATE */
#  endif   /* !defined(__WAYLAND_DYNLOAD_CLIENT_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE) */
#endif     /* !defined(WAYLAND_DYNLOAD_FN) && !defined(WAYLAND_DYNLOAD_IFACE) */

#ifdef __cplusplus
}
#endif
