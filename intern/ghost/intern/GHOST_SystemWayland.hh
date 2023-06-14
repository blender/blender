/* SPDX-FileCopyrightText: 2020-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_SystemWayland class.
 */

#pragma once

#include "../GHOST_Types.h"
#include "GHOST_System.hh"
#include "GHOST_WindowWayland.hh"

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

/**
 * Enter/exit handlers may be called with a null window surface (when the window just closed),
 * so add a version of the function that checks this.
 *
 * All of the functions could in fact however paranoid null checks make the expected
 * state difficult to reason about, so only use this in cases the surface may be null.
 */
bool ghost_wl_surface_own_with_null_check(const struct wl_surface *wl_surface);
bool ghost_wl_surface_own(const struct wl_surface *wl_surface);
void ghost_wl_surface_tag(struct wl_surface *wl_surface);
GHOST_WindowWayland *ghost_wl_surface_user_data(struct wl_surface *wl_surface);

bool ghost_wl_surface_own_cursor_pointer(const struct wl_surface *wl_surface);
void ghost_wl_surface_tag_cursor_pointer(struct wl_surface *wl_surface);

bool ghost_wl_surface_own_cursor_tablet(const struct wl_surface *wl_surface);
void ghost_wl_surface_tag_cursor_tablet(struct wl_surface *wl_surface);

/* Scaling to: translates from WAYLAND into GHOST (viewport local) coordinates.
 * Scaling from: performs the reverse translation.
 *
 * Scaling "to" is used to map WAYLAND location cursor coordinates to GHOST coordinates.
 * Scaling "from" is used to clamp cursor coordinates in WAYLAND local coordinates. */

struct GWL_WindowScaleParams;
wl_fixed_t gwl_window_scale_wl_fixed_to(const GWL_WindowScaleParams &scale_params,
                                        wl_fixed_t value);
wl_fixed_t gwl_window_scale_wl_fixed_from(const GWL_WindowScaleParams &scale_params,
                                          wl_fixed_t value);

/* Avoid this where possible as scaling integers often incurs rounding errors.
 * Scale #wl_fixed_t where possible.
 *
 * In general scale by large values where this is less likely to be a problem. */

int gwl_window_scale_int_to(const GWL_WindowScaleParams &scale_params, int value);
int gwl_window_scale_int_from(const GWL_WindowScaleParams &scale_params, int value);

#define FRACTIONAL_DENOMINATOR 120

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
   * \note Use the same scale as #wp_fractional_scale_manager_v1
   * (avoid floating point arithmetic in general).
   */
  int scale_fractional = (1 * FRACTIONAL_DENOMINATOR);
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

  GHOST_IContext *createOffscreenContext(GHOST_GPUSettings gpuSettings) override;

  GHOST_TSuccess disposeContext(GHOST_IContext *context) override;

  GHOST_IWindow *createWindow(const char *title,
                              int32_t left,
                              int32_t top,
                              uint32_t width,
                              uint32_t height,
                              GHOST_TWindowState state,
                              GHOST_GPUSettings gpuSettings,
                              const bool exclusive,
                              const bool is_dialog,
                              const GHOST_IWindow *parentWindow) override;

  GHOST_TCapabilityFlag getCapabilities() const override;

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

#ifdef USE_EVENT_BACKGROUND_THREAD
  /**
   * Return a separate WAYLAND local timer manager to #GHOST_System::getTimerManager
   * Manipulation & access must lock with #GHOST_WaylandSystem::server_mutex.
   *
   * See #GWL_Display::ghost_timer_manager doc-string for details on why this is needed.
   */
  GHOST_TimerManager *ghost_timer_manager();
#endif

  /* WAYLAND direct-data access. */

  struct wl_display *wl_display();
  struct wl_compositor *wl_compositor();
  struct zwp_primary_selection_device_manager_v1 *wp_primary_selection_manager();
  struct xdg_activation_v1 *xdg_activation_manager();
  struct zwp_pointer_gestures_v1 *wp_pointer_gestures();
  struct wp_fractional_scale_manager_v1 *wp_fractional_scale_manager();
  struct wp_viewporter *wp_viewporter();

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor *libdecor_context();
#endif
  struct xdg_wm_base *xdg_decor_shell();
  struct zxdg_decoration_manager_v1 *xdg_decor_manager();
  /* End `xdg_decor`. */

  const std::vector<GWL_Output *> &outputs() const;

  struct wl_shm *wl_shm() const;

  static const char *xdg_app_id();

  /* WAYLAND utility functions. */

  /**
   * Push an event, with support for calling from a thread.
   * NOTE: only needed for `USE_EVENT_BACKGROUND_THREAD`.
   */
  GHOST_TSuccess pushEvent_maybe_pending(GHOST_IEvent *event);

  /** Set this seat to be active. */
  void seat_active_set(const struct GWL_Seat *seat);

  struct wl_seat *wl_seat_active_get_with_input_serial(uint32_t &serial);

  /**
   * Clear all references to this output.
   *
   * \note The compositor should have already called the `wl_surface_listener.leave` callback,
   * however some compositors may not (see #103586).
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
                              const struct GWL_WindowScaleParams &scale_params);

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  static bool use_libdecor_runtime();
#endif

#ifdef USE_EVENT_BACKGROUND_THREAD
  /* NOTE: allocate mutex so `const` functions can lock the mutex. */

  /** Lock to prevent #wl_display_dispatch / #wl_display_roundtrip / #wl_display_flush
   * from running at the same time. */
  std::mutex *server_mutex = nullptr;

  /**
   * Threads must lock this before manipulating #GWL_Display::ghost_timer_manager.
   *
   * \note Using a separate lock to `server_mutex` is necessary because the
   * server lock is already held when calling `ghost_wl_display_event_pump`.
   * If manipulating the timer used the `server_mutex`, event pump can indirectly
   * handle key up/down events which would lock `server_mutex` causing a dead-lock.
   */
  std::mutex *timer_mutex = nullptr;

  std::thread::id main_thread_id;

  std::atomic<bool> has_pending_actions_for_window = false;
#endif

 private:
  /**
   * Support freeing the internal data separately from the destructor
   * so it can be called when WAYLAND isn't running (immediately before raising an exception).
   */
  void display_destroy_and_free_all();

  struct GWL_Display *display_;
};
