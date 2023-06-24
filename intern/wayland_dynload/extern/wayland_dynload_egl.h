/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-egl.h>`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WAYLAND_DYNLOAD_FN
WAYLAND_DYNLOAD_FN(wl_egl_window_create)
WAYLAND_DYNLOAD_FN(wl_egl_window_destroy)
WAYLAND_DYNLOAD_FN(wl_egl_window_resize)
WAYLAND_DYNLOAD_FN(wl_egl_window_get_attached_size)
#elif defined(WAYLAND_DYNLOAD_IFACE)
/* No interfaces. */
#else

/* Header guard. */
#  if !defined(__WAYLAND_DYNLOAD_EGL_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE)
#    define __WAYLAND_DYNLOAD_EGL_H__

#    include <wayland-egl-core.h>
extern struct WaylandDynload_EGL wayland_dynload_egl;

/* Support validating declarations against the header. */
#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define WL_DYN_FN(a) (*a)
#    else
#      define WL_DYN_FN(a) (a)
#    endif

#    ifndef WAYLAND_DYNLOAD_VALIDATE
struct WaylandDynload_EGL {
#    endif

  struct wl_egl_window *WL_DYN_FN(wl_egl_window_create)(struct wl_surface *surface,
                                                        int width,
                                                        int height);
  void WL_DYN_FN(wl_egl_window_destroy)(struct wl_egl_window *egl_window);
  void WL_DYN_FN(wl_egl_window_resize)(
      struct wl_egl_window *egl_window, int width, int height, int dx, int dy);
  void WL_DYN_FN(wl_egl_window_get_attached_size)(struct wl_egl_window *egl_window,
                                                  int *width,
                                                  int *height);

#    ifndef WAYLAND_DYNLOAD_VALIDATE
};
#    endif
#    undef WL_DYN_FN

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define wl_egl_window_create(...) (*wayland_dynload_egl.wl_egl_window_create)(__VA_ARGS__)
#      define wl_egl_window_destroy(...) (*wayland_dynload_egl.wl_egl_window_destroy)(__VA_ARGS__)
#      define wl_egl_window_resize(...) (*wayland_dynload_egl.wl_egl_window_resize)(__VA_ARGS__)
#      define wl_egl_window_get_attached_size(...) \
        (*wayland_dynload_egl.wl_egl_window_get_attached_size)(__VA_ARGS__)

#    endif /* !WAYLAND_DYNLOAD_VALIDATE */
#  endif   /* !defined(WAYLAND_DYNLOAD_FN) && !defined(WAYLAND_DYNLOAD_IFACE) */
#endif     /* !defined(__WAYLAND_DYNLOAD_EGL_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE) */

#ifdef __cplusplus
}
#endif
