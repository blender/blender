/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-cursor.h>`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WAYLAND_DYNLOAD_FN
WAYLAND_DYNLOAD_FN(wl_cursor_theme_load)
WAYLAND_DYNLOAD_FN(wl_cursor_theme_destroy)
WAYLAND_DYNLOAD_FN(wl_cursor_theme_get_cursor)
WAYLAND_DYNLOAD_FN(wl_cursor_image_get_buffer)
WAYLAND_DYNLOAD_FN(wl_cursor_frame)
WAYLAND_DYNLOAD_FN(wl_cursor_frame_and_duration)
#elif defined(WAYLAND_DYNLOAD_IFACE)
/* No interfaces. */
#else

/* Header guard. */
#  if !defined(__WAYLAND_DYNLOAD_CURSOR_H__) && !defined(WAYLAND_DYNLOAD_VALIDATE)
#    define __WAYLAND_DYNLOAD_CURSOR_H__

#    include <wayland-cursor.h>
extern struct WaylandDynload_Cursor wayland_dynload_cursor;

/* Support validating declarations against the header. */
#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define WL_DYN_FN(a) (*a)
#    else
#      define WL_DYN_FN(a) (a)
#    endif

#    ifndef WAYLAND_DYNLOAD_VALIDATE
struct WaylandDynload_Cursor {
#    endif

  struct wl_cursor_theme *WL_DYN_FN(wl_cursor_theme_load)(const char *name,
                                                          int size,
                                                          struct wl_shm *shm);
  void WL_DYN_FN(wl_cursor_theme_destroy)(struct wl_cursor_theme *theme);
  struct wl_cursor *WL_DYN_FN(wl_cursor_theme_get_cursor)(struct wl_cursor_theme *theme,
                                                          const char *name);
  struct wl_buffer *WL_DYN_FN(wl_cursor_image_get_buffer)(struct wl_cursor_image *image);
  int WL_DYN_FN(wl_cursor_frame)(struct wl_cursor *cursor, uint32_t time);
  int WL_DYN_FN(wl_cursor_frame_and_duration)(struct wl_cursor *cursor,
                                              uint32_t time,
                                              uint32_t *duration);

#    ifndef WAYLAND_DYNLOAD_VALIDATE
};
#    endif
#    undef WL_DYN_FN

#    ifndef WAYLAND_DYNLOAD_VALIDATE
#      define wl_cursor_theme_load(...) (*wayland_dynload_cursor.wl_cursor_theme_load)(__VA_ARGS__)
#      define wl_cursor_theme_destroy(...) \
        (*wayland_dynload_cursor.wl_cursor_theme_destroy)(__VA_ARGS__)
#      define wl_cursor_theme_get_cursor(...) \
        (*wayland_dynload_cursor.wl_cursor_theme_get_cursor)(__VA_ARGS__)
#      define wl_cursor_image_get_buffer(...) \
        (*wayland_dynload_cursor.wl_cursor_image_get_buffer)(__VA_ARGS__)
#      define wl_cursor_frame(...) (*wayland_dynload_cursor.wl_cursor_frame)(__VA_ARGS__)
#      define wl_cursor_frame_and_duration(...) \
        (*wayland_dynload_cursor.wl_cursor_frame_and_duration)(__VA_ARGS__)
#    endif /* !WAYLAND_DYNLOAD_VALIDATE */
#  endif   /* !__WAYLAND_DYNLOAD_CLIENT_H__ */
#endif     /* !defined(WAYLAND_DYNLOAD_FN) && !defined(WAYLAND_DYNLOAD_IFACE) */

#ifdef __cplusplus
}
#endif
