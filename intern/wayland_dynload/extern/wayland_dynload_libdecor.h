/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<libdecor.h>`.
 *
 * \note Not part of WAYLAND, but used with WAYLAND by GHOST.
 * It follows WAYLAND conventions and is a middle-ware library that depends on `libwayland-client`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WAYLAND_DYNLOAD_FN
WAYLAND_DYNLOAD_FN(libdecor_configuration_get_content_size)
WAYLAND_DYNLOAD_FN(libdecor_configuration_get_window_state)
WAYLAND_DYNLOAD_FN(libdecor_decorate)
WAYLAND_DYNLOAD_FN(libdecor_dispatch)
WAYLAND_DYNLOAD_FN(libdecor_frame_commit)
WAYLAND_DYNLOAD_FN(libdecor_frame_get_xdg_toplevel)
WAYLAND_DYNLOAD_FN(libdecor_frame_map)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_app_id)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_fullscreen)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_maximized)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_min_content_size)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_minimized)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_parent)
WAYLAND_DYNLOAD_FN(libdecor_frame_set_title)
WAYLAND_DYNLOAD_FN(libdecor_frame_unref)
WAYLAND_DYNLOAD_FN(libdecor_frame_unset_fullscreen)
WAYLAND_DYNLOAD_FN(libdecor_frame_unset_maximized)
WAYLAND_DYNLOAD_FN(libdecor_new)
WAYLAND_DYNLOAD_FN(libdecor_state_free)
WAYLAND_DYNLOAD_FN(libdecor_state_new)
WAYLAND_DYNLOAD_FN(libdecor_unref)
#elif defined(WAYLAND_DYNLOAD_IFACE)
/* No interfaces. */
#else

/* Header guard. */
#  if !defined(__WAYLAND_DYNLOAD_LIBDECOR_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE)
#    define __WAYLAND_DYNLOAD_LIBDECOR_H__

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      include <libdecor.h>
extern struct WaylandDynload_Libdecor wayland_dynload_libdecor;
#    endif

/* Support validating declarations against the header. */
#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define WL_DYN_FN(sym) (*sym)
#    else
#      define WL_DYN_FN(sym) (sym)
#    endif

#    ifndef WAYLAND_DYNLOAD_VALIDATE
struct WaylandDynload_Libdecor {
#    endif

  bool WL_DYN_FN(libdecor_configuration_get_content_size)(
      struct libdecor_configuration *configuration,
      struct libdecor_frame *frame,
      int *width,
      int *height);
  bool WL_DYN_FN(libdecor_configuration_get_window_state)(
      struct libdecor_configuration *configuration, enum libdecor_window_state *window_state);
  struct libdecor_frame *WL_DYN_FN(libdecor_decorate)(struct libdecor *context,
                                                      struct wl_surface *surface,
                                                      struct libdecor_frame_interface *iface,
                                                      void *user_data);
  int WL_DYN_FN(libdecor_dispatch)(struct libdecor *context, int timeout);
  void WL_DYN_FN(libdecor_frame_commit)(struct libdecor_frame *frame,
                                        struct libdecor_state *state,
                                        struct libdecor_configuration *configuration);
  struct xdg_toplevel *WL_DYN_FN(libdecor_frame_get_xdg_toplevel)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_map)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_set_app_id)(struct libdecor_frame *frame, const char *app_id);
  void WL_DYN_FN(libdecor_frame_set_fullscreen)(struct libdecor_frame *frame,
                                                struct wl_output *output);
  void WL_DYN_FN(libdecor_frame_set_maximized)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_set_min_content_size)(struct libdecor_frame *frame,
                                                      int content_width,
                                                      int content_height);
  void WL_DYN_FN(libdecor_frame_set_minimized)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_set_parent)(struct libdecor_frame *frame,
                                            struct libdecor_frame *parent);
  void WL_DYN_FN(libdecor_frame_set_title)(struct libdecor_frame *frame, const char *title);
  void WL_DYN_FN(libdecor_frame_unref)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_unset_fullscreen)(struct libdecor_frame *frame);
  void WL_DYN_FN(libdecor_frame_unset_maximized)(struct libdecor_frame *frame);
  struct libdecor *WL_DYN_FN(libdecor_new)(struct wl_display *display,
                                           struct libdecor_interface *iface);
  void WL_DYN_FN(libdecor_state_free)(struct libdecor_state *state);
  struct libdecor_state *WL_DYN_FN(libdecor_state_new)(int width, int height);
  void WL_DYN_FN(libdecor_unref)(struct libdecor *context);

#    ifndef WAYLAND_DYNLOAD_VALIDATE
};
#    endif
#    undef WL_DYN_FN

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define libdecor_configuration_get_content_size(...) \
        (*wayland_dynload_libdecor.libdecor_configuration_get_content_size)(__VA_ARGS__)
#      define libdecor_configuration_get_window_state(...) \
        (*wayland_dynload_libdecor.libdecor_configuration_get_window_state)(__VA_ARGS__)
#      define libdecor_decorate(...) (*wayland_dynload_libdecor.libdecor_decorate)(__VA_ARGS__)
#      define libdecor_dispatch(...) (*wayland_dynload_libdecor.libdecor_dispatch)(__VA_ARGS__)
#      define libdecor_frame_commit(...) \
        (*wayland_dynload_libdecor.libdecor_frame_commit)(__VA_ARGS__)
#      define libdecor_frame_get_xdg_toplevel(...) \
        (*wayland_dynload_libdecor.libdecor_frame_get_xdg_toplevel)(__VA_ARGS__)
#      define libdecor_frame_map(...) (*wayland_dynload_libdecor.libdecor_frame_map)(__VA_ARGS__)
#      define libdecor_frame_set_app_id(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_app_id)(__VA_ARGS__)
#      define libdecor_frame_set_fullscreen(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_fullscreen)(__VA_ARGS__)
#      define libdecor_frame_set_maximized(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_maximized)(__VA_ARGS__)
#      define libdecor_frame_set_min_content_size(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_min_content_size)(__VA_ARGS__)
#      define libdecor_frame_set_minimized(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_minimized)(__VA_ARGS__)
#      define libdecor_frame_set_parent(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_parent)(__VA_ARGS__)
#      define libdecor_frame_set_title(...) \
        (*wayland_dynload_libdecor.libdecor_frame_set_title)(__VA_ARGS__)
#      define libdecor_frame_unref(...) \
        (*wayland_dynload_libdecor.libdecor_frame_unref)(__VA_ARGS__)
#      define libdecor_frame_unset_fullscreen(...) \
        (*wayland_dynload_libdecor.libdecor_frame_unset_fullscreen)(__VA_ARGS__)
#      define libdecor_frame_unset_maximized(...) \
        (*wayland_dynload_libdecor.libdecor_frame_unset_maximized)(__VA_ARGS__)
#      define libdecor_new(...) (*wayland_dynload_libdecor.libdecor_new)(__VA_ARGS__)
#      define libdecor_state_free(...) (*wayland_dynload_libdecor.libdecor_state_free)(__VA_ARGS__)
#      define libdecor_state_new(...) (*wayland_dynload_libdecor.libdecor_state_new)(__VA_ARGS__)
#      define libdecor_unref(...) (*wayland_dynload_libdecor.libdecor_unref)(__VA_ARGS__)

#    endif /* !WAYLAND_DYNLOAD_VALIDATE */
#  endif   /* !defined(__WAYLAND_DYNLOAD_LIBDECOR_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE) */
#endif     /* !defined(WAYLAND_DYNLOAD_FN) && !defined(WAYLAND_DYNLOAD_IFACE) */

#ifdef __cplusplus
}
#endif
