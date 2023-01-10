/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemWayland class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_System.h"
#include "GHOST_WindowWayland.h"

#ifdef WITH_GHOST_WAYLAND_DYNLOAD
#  include <wayland_dynload_client.h>
#endif
#include <wayland-client.h>

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
#  ifdef WITH_GHOST_WAYLAND_DYNLOAD
#    include <wayland_dynload_libdecor.h>
#  endif
#  include <libdecor.h>
#endif

#include <mutex>
#include <string>

#ifdef USE_EVENT_BACKGROUND_THREAD
#  include <atomic>
#  include <thread>
#endif

class GHOST_WindowWayland;

bool ghost_wl_output_own(const struct wl_output *wl_output);
void ghost_wl_output_tag(struct wl_output *wl_output);
struct GWL_Output *ghost_wl_output_user_data(struct wl_output *wl_output);

bool ghost_wl_surface_own(const struct wl_surface *surface);
void ghost_wl_surface_tag(struct wl_surface *surface);
GHOST_WindowWayland *ghost_wl_surface_user_data(struct wl_surface *surface);

bool ghost_wl_surface_own_cursor_pointer(const struct wl_surface *surface);
void ghost_wl_surface_tag_cursor_pointer(struct wl_surface *surface);

bool ghost_wl_surface_own_cursor_tablet(const struct wl_surface *surface);
void ghost_wl_surface_tag_cursor_tablet(struct wl_surface *surface);

#ifdef WITH_GHOST_WAYLAND_DYNLOAD
/**
 * Return true when all required WAYLAND libraries are present,
 * Performs dynamic loading when `WITH_GHOST_WAYLAND_DYNLOAD` is in use.
 */
bool ghost_wl_dynload_libraries_init();
void ghost_wl_dynload_libraries_exit();
#endif

struct GWL_Output {
  GHOST_SystemWayland *system = nullptr;

  struct wl_output *wl_output = nullptr;
  struct zxdg_output_v1 *xdg_output = nullptr;
  /** Dimensions in pixels. */
  int32_t size_native[2] = {0, 0};
  /** Dimensions in millimeter. */
  int32_t size_mm[2] = {0, 0};

  int32_t size_logical[2] = {0, 0};
  bool has_size_logical = false;

  /** Monitor position in pixels. */
  int32_t position_logical[2] = {0, 0};
  bool has_position_logical = false;

  int transform = 0;
  int scale = 1;
  /**
   * The integer `scale` value should be used in almost all cases,
   * as this is what is used for most API calls.
   * Only use fractional scaling to calculate the DPI.
   *
   * \note Internally an #wl_fixed_t is used to store the scale of the display,
   * so use the same value here (avoid floating point arithmetic in general).
   */
  wl_fixed_t scale_fractional = wl_fixed_from_int(1);
  bool has_scale_fractional = false;

  std::string make;
  std::string model;
};

class GHOST_SystemWayland : public GHOST_System {
 public:
  GHOST_SystemWayland(bool background);
  GHOST_SystemWayland() : GHOST_SystemWayland(true){};

  ~GHOST_SystemWayland() override;

  GHOST_TSuccess init() override;

  bool processEvents(bool waitForEvent) override;

  bool setConsoleWindowState(GHOST_TConsoleWindowState action) override;

  GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys &keys) const override;

  GHOST_TSuccess getButtons(GHOST_Buttons &buttons) const override;

  char *getClipboard(bool selection) const override;

  void putClipboard(const char *buffer, bool selection) const override;

  uint8_t getNumDisplays() const override;

  GHOST_TSuccess getCursorPositionClientRelative(const GHOST_IWindow *window,
                                                 int32_t &x,
                                                 int32_t &y) const override;
  GHOST_TSuccess setCursorPositionClientRelative(GHOST_IWindow *window,
                                                 int32_t x,
                                                 int32_t y) override;

  GHOST_TSuccess getCursorPosition(int32_t &x, int32_t &y) const override;
  GHOST_TSuccess setCursorPosition(int32_t x, int32_t y) override;

  void getMainDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  void getAllDisplayDimensions(uint32_t &width, uint32_t &height) const override;

  GHOST_IContext *createOffscreenContext(GHOST_GLSettings glSettings) override;

  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GLSettings glSettings,
                              const bool exclusive,
                              const bool is_dialog,
                              const GHOST_IWindow *parentWindow) override;

  bool supportsCursorWarp() override;
  bool supportsWindowPosition() override;

  /* WAYLAND utility functions (share window/system logic). */

  GHOST_TSuccess cursor_shape_set(GHOST_TStandardCursor shape);

  GHOST_TSuccess cursor_shape_check(GHOST_TStandardCursor cursorShape);

  GHOST_TSuccess cursor_shape_custom_set(uint8_t *bitmap,
                                         uint8_t *mask,
                                         int sizex,
                                         int sizey,
                                         int hotX,
                                         int hotY,
                                         bool canInvertColor);

  GHOST_TSuccess cursor_bitmap_get(GHOST_CursorBitmapRef *bitmap);

  GHOST_TSuccess cursor_visibility_set(bool visible);

  bool cursor_grab_use_software_display_get(const GHOST_TGrabCursorMode mode);

  /* WAYLAND direct-data access. */

  struct wl_display *wl_display();
  struct wl_compositor *wl_compositor();
  struct zwp_primary_selection_device_manager_v1 *wp_primary_selection_manager();
  struct zwp_pointer_gestures_v1 *wp_pointer_gestures();

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor *libdecor_context();
#endif
  struct xdg_wm_base *xdg_decor_shell();
  struct zxdg_decoration_manager_v1 *xdg_decor_manager();
  /* End `xdg_decor`. */

  const std::vector<GWL_Output *> &outputs() const;

  struct wl_shm *wl_shm() const;

  /* WAYLAND utility functions. */

  /**
   * Push an event, with support for calling from a thread.
   * NOTE: only needed for `USE_EVENT_BACKGROUND_THREAD`.
   */
  GHOST_TSuccess pushEvent_maybe_pending(GHOST_IEvent *event);

  /** Set this seat to be active. */
  void seat_active_set(const struct GWL_Seat *seat);

  /**
   * Clear all references to this output.
   *
   * \note The compositor should have already called the `wl_surface_listener.leave` callback,
   * however some compositors may not (see T103586).
   * So remove references to the output before it's destroyed to avoid crashing.
   *
   * \return true when any references were removed.
   */
  bool output_unref(struct wl_output *wl_output);

  void output_scale_update(GWL_Output *output);

  /**
   * Clear all references to this surface to prevent accessing NULL pointers.
   *
   * \return true when any references were removed.
   */
  bool window_surface_unref(const wl_surface *wl_surface);

  bool window_cursor_grab_set(const GHOST_TGrabCursorMode mode,
                              const GHOST_TGrabCursorMode mode_current,
                              int32_t init_grab_xy[2],
                              const GHOST_Rect *wrap_bounds,
                              GHOST_TAxisFlag wrap_axis,
                              wl_surface *wl_surface,
                              int scale);

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  static bool use_libdecor_runtime();
#endif

#ifdef USE_EVENT_BACKGROUND_THREAD
  /* NOTE: allocate mutex so `const` functions can lock the mutex. */

  /** Lock to prevent #wl_display_dispatch / #wl_display_roundtrip / #wl_display_flush
   * from running at the same time. */
  std::mutex *server_mutex = nullptr;

  /** Threads must lock this before manipulating timers. */
  std::mutex *timer_mutex = nullptr;

  std::thread::id main_thread_id;

  std::atomic<bool> has_pending_actions_for_window = false;
#endif

 private:
  struct GWL_Display *display_;
};
