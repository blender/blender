/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Declaration of GHOST_WindowWayland class.
 */

#pragma once

#include "GHOST_Window.h"

#include <vector>

#include <wayland-util.h> /* For #wl_fixed_t */

/**
 * Define to workaround for a bug/limitation in WAYLAND, see: #100855 & upstream report:
 * https://gitlab.freedesktop.org/wayland/wayland/-/issues/159
 *
 * Consume events from WAYLAND in a thread, this is needed because overflowing the event queue
 * causes a fatal error (more than `sizeof(wl_buffer.data)` at the time of writing).
 *
 * Solve this using a thread that handles events, locking must be performed as follows:
 *
 * - Lock #GWL_Display.server_mutex to prevent #wl_display_dispatch / #wl_display_roundtrip
 *   running from multiple threads at once.
 *
 *   Even though WAYLAND functions that communicate with `wl_display_*` have their own locks,
 *   GHOST functions that communicate with WAYLAND must use this lock to be thread safe.
 *
 *   Without this reading/writing values such as changing the cursor or setting the window size
 *   could conflict with WAYLAND callbacks running in a separate thread.
 *   So the `server_mutex` ensures either GHOST or WAYLAND is manipulating this data,
 *   having two WAYLAND callbacks accessing the data at once isn't a problem.
 *
 *   \warning Some operations such as #ContextEGL creation/deletion & swap-buffers may call
 *   #wl_display_dispatch indirectly, so it's important to take care to lock the `server_mutex`,
 *   before accessing these functions too.
 *
 *   \warning An unfortunate side-effect of this is care needs to be taken not to call public
 *   GHOST functions from each other in situations that would otherwise be supported.
 *   As both using a `server_mutex` results in a dead-lock. In some cases this means the
 *   implementation and the public function may need to be split.
 *
 * - Lock #GWL_Display.timer_mutex when WAYLAND callbacks manipulate timers.
 *
 * - Lock #GWL_Display.events_pending_mutex before manipulating #GWL_Display.events_pending.
 *
 * - Lock #GWL_Window.frame_pending_mutex before changing window size & frame settings,
 *   this is flushed in #GHOST_WindowWayland::pending_actions_handle.
 */
#define USE_EVENT_BACKGROUND_THREAD

class GHOST_SystemWayland;

struct GWL_Output;
struct GWL_Window;

class GHOST_WindowWayland : public GHOST_Window {
 public:
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor cursorShape) override;

  GHOST_WindowWayland(GHOST_SystemWayland *system,
                      const char *title,
                      int32_t left,
                      int32_t top,
                      uint32_t width,
                      uint32_t height,
                      GHOST_TWindowState state,
                      const GHOST_IWindow *parentWindow,
                      GHOST_TDrawingContextType type,
                      const bool is_dialog,
                      const bool stereoVisual,
                      const bool exclusive);

  ~GHOST_WindowWayland() override;

  /* Ghost API */

#ifdef USE_EVENT_BACKGROUND_THREAD
  GHOST_TSuccess swapBuffers() override; /* Only for assertion. */
#endif

  uint16_t getDPIHint() override;

  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) override;

  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;

  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor) override;
  bool getCursorGrabUseSoftwareDisplay() override;

  GHOST_TSuccess getCursorBitmap(GHOST_CursorBitmapRef *bitmap) override;

  void setTitle(const char *title) override;

  std::string getTitle() const override;

  void getWindowBounds(GHOST_Rect &bounds) const override;

  void getClientBounds(GHOST_Rect &bounds) const override;

  GHOST_TSuccess setClientWidth(uint32_t width) override;

  GHOST_TSuccess setClientHeight(uint32_t height) override;

  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) override;

  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  GHOST_TSuccess setWindowCursorVisibility(bool visible) override;

  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  GHOST_TWindowState getState() const override;

  GHOST_TSuccess invalidate() override;

  GHOST_TSuccess setOrder(GHOST_TWindowOrder order) override;

  GHOST_TSuccess beginFullScreen() const override;

  GHOST_TSuccess endFullScreen() const override;

  bool isDialog() const override;

#ifdef GHOST_OPENGL_ALPHA
  void setOpaque() const;
#endif

  /* WAYLAND direct-data access. */

  int scale() const;
  wl_fixed_t scale_fractional() const;
  struct wl_surface *wl_surface() const;
  const std::vector<GWL_Output *> &outputs();

  /* WAYLAND window-level functions. */

  GHOST_TSuccess close();
  GHOST_TSuccess activate();
  GHOST_TSuccess deactivate();
  GHOST_TSuccess notify_size();
  GHOST_TSuccess notify_decor_redraw();

  /* WAYLAND utility functions. */

  bool outputs_enter(GWL_Output *output);
  bool outputs_leave(GWL_Output *output);

  /**
   * Return true when the windows scale or DPI changes.
   */
  bool outputs_changed_update_scale();

#ifdef USE_EVENT_BACKGROUND_THREAD
  void pending_actions_handle();
#endif

 private:
  GHOST_SystemWayland *system_;
  struct GWL_Window *window_;

  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;
};
