/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_WindowWayland.h"
#include "GHOST_SystemWayland.h"
#include "GHOST_WaylandUtils.h"
#include "GHOST_WindowManager.h"

#include "GHOST_Event.h"

#include "GHOST_ContextEGL.h"
#include "GHOST_ContextNone.h"

#include <wayland-egl.h>

#include <algorithm> /* For `std::find`. */

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
#  include <libdecor.h>
#endif

static constexpr size_t base_dpi = 96;

struct window_t {
  GHOST_WindowWayland *w = nullptr;
  struct wl_surface *wl_surface = nullptr;
  /**
   * Outputs on which the window is currently shown on.
   *
   * This is an ordered set (whoever adds to this is responsible for keeping members unique).
   * In practice this is rarely manipulated and is limited by the number of physical displays.
   */
  std::vector<output_t *> outputs;

  /** The scale value written to #wl_surface_set_buffer_scale. */
  int scale = 0;
  /**
   * The DPI, either:
   * - `scale * base_dpi`
   * - `wl_fixed_to_int(scale_fractional * base_dpi)`
   * When fractional scaling is available.
   */
  uint32_t dpi = 0;

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  struct libdecor_frame *decor_frame = nullptr;
  bool decor_configured = false;
#else
  struct xdg_surface *xdg_surface = nullptr;
  struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration = nullptr;
  struct xdg_toplevel *xdg_toplevel = nullptr;

  enum zxdg_toplevel_decoration_v1_mode decoration_mode = (enum zxdg_toplevel_decoration_v1_mode)0;
#endif

  wl_egl_window *egl_window = nullptr;
  bool is_maximised = false;
  bool is_fullscreen = false;
  bool is_active = false;
  bool is_dialog = false;

  int32_t size[2] = {0, 0};
  int32_t size_pending[2] = {0, 0};
};

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Return -1 if `output_a` has a scale smaller than `output_b`, 0 when there equal, otherwise 1.
 */
static int output_scale_cmp(const output_t *output_a, const output_t *output_b)
{
  if (output_a->scale < output_b->scale) {
    return -1;
  }
  if (output_a->scale > output_b->scale) {
    return 1;
  }
  if (output_a->has_scale_fractional || output_b->has_scale_fractional) {
    const wl_fixed_t scale_fractional_a = output_a->has_scale_fractional ?
                                              output_a->scale_fractional :
                                              wl_fixed_from_int(output_a->scale);
    const wl_fixed_t scale_fractional_b = output_b->has_scale_fractional ?
                                              output_b->scale_fractional :
                                              wl_fixed_from_int(output_b->scale);
    if (scale_fractional_a < scale_fractional_b) {
      return -1;
    }
    if (scale_fractional_a > scale_fractional_b) {
      return 1;
    }
  }
  return 0;
}

static int outputs_max_scale_or_default(const std::vector<output_t *> &outputs,
                                        const int32_t scale_default,
                                        uint32_t *r_dpi)
{
  const output_t *output_max = nullptr;
  for (const output_t *reg_output : outputs) {
    if (!output_max || (output_scale_cmp(output_max, reg_output) == -1)) {
      output_max = reg_output;
    }
  }

  if (output_max) {
    if (r_dpi) {
      *r_dpi = output_max->has_scale_fractional ?
                   /* Fractional DPI. */
                   wl_fixed_to_int(output_max->scale_fractional * base_dpi) :
                   /* Simple non-fractional DPI. */
                   (output_max->scale * base_dpi);
    }
    return output_max->scale;
  }

  if (r_dpi) {
    *r_dpi = scale_default * base_dpi;
  }
  return scale_default;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG Top Level), #xdg_toplevel_listener
 * \{ */

#ifndef WITH_GHOST_WAYLAND_LIBDECOR

static void xdg_toplevel_handle_configure(void *data,
                                          xdg_toplevel * /*xdg_toplevel*/,
                                          const int32_t width,
                                          const int32_t height,
                                          wl_array *states)
{
  window_t *win = static_cast<window_t *>(data);
  win->size_pending[0] = win->scale * width;
  win->size_pending[1] = win->scale * height;

  win->is_maximised = false;
  win->is_fullscreen = false;
  win->is_active = false;

  enum xdg_toplevel_state *state;
  WL_ARRAY_FOR_EACH (state, states) {
    switch (*state) {
      case XDG_TOPLEVEL_STATE_MAXIMIZED:
        win->is_maximised = true;
        break;
      case XDG_TOPLEVEL_STATE_FULLSCREEN:
        win->is_fullscreen = true;
        break;
      case XDG_TOPLEVEL_STATE_ACTIVATED:
        win->is_active = true;
        break;
      default:
        break;
    }
  }
}

static void xdg_toplevel_handle_close(void *data, xdg_toplevel * /*xdg_toplevel*/)
{
  static_cast<window_t *>(data)->w->close();
}

static const xdg_toplevel_listener toplevel_listener = {
    xdg_toplevel_handle_configure,
    xdg_toplevel_handle_close,
};

#endif /* !WITH_GHOST_WAYLAND_LIBDECOR. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (LibDecor Frame), #libdecor_frame_interface
 * \{ */

#ifdef WITH_GHOST_WAYLAND_LIBDECOR

static void frame_handle_configure(struct libdecor_frame *frame,
                                   struct libdecor_configuration *configuration,
                                   void *data)
{
  window_t *win = static_cast<window_t *>(data);

  int size_next[2];
  enum libdecor_window_state window_state;
  struct libdecor_state *state;

  if (!libdecor_configuration_get_content_size(
          configuration, frame, &size_next[0], &size_next[1])) {
    size_next[0] = win->size[0] / win->scale;
    size_next[1] = win->size[1] / win->scale;
  }

  win->size[0] = win->scale * size_next[0];
  win->size[1] = win->scale * size_next[1];

  wl_egl_window_resize(win->egl_window, win->size[0], win->size[1], 0, 0);
  win->w->notify_size();

  if (!libdecor_configuration_get_window_state(configuration, &window_state)) {
    window_state = LIBDECOR_WINDOW_STATE_NONE;
  }

  win->is_maximised = window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED;
  win->is_fullscreen = window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN;
  win->is_active = window_state & LIBDECOR_WINDOW_STATE_ACTIVE;

  win->is_active ? win->w->activate() : win->w->deactivate();

  state = libdecor_state_new(size_next[0], size_next[1]);
  libdecor_frame_commit(frame, state, configuration);
  libdecor_state_free(state);

  win->decor_configured = true;
}

static void frame_handle_close(struct libdecor_frame * /*frame*/, void *data)
{
  static_cast<window_t *>(data)->w->close();
}

static void frame_handle_commit(struct libdecor_frame * /*frame*/, void *data)
{
  /* We have to swap twice to keep any pop-up menus alive. */
  static_cast<window_t *>(data)->w->swapBuffers();
  static_cast<window_t *>(data)->w->swapBuffers();
}

static struct libdecor_frame_interface libdecor_frame_iface = {
    frame_handle_configure,
    frame_handle_close,
    frame_handle_commit,
};

#endif /* WITH_GHOST_WAYLAND_LIBDECOR. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG Decoration Listener), #zxdg_toplevel_decoration_v1_listener
 * \{ */

#ifndef WITH_GHOST_WAYLAND_LIBDECOR

static void xdg_toplevel_decoration_handle_configure(
    void *data,
    struct zxdg_toplevel_decoration_v1 * /*zxdg_toplevel_decoration_v1*/,
    const uint32_t mode)
{
  static_cast<window_t *>(data)->decoration_mode = zxdg_toplevel_decoration_v1_mode(mode);
}

static const zxdg_toplevel_decoration_v1_listener toplevel_decoration_v1_listener = {
    xdg_toplevel_decoration_handle_configure,
};

#endif /* !WITH_GHOST_WAYLAND_LIBDECOR. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG Surface Handle Configure), #xdg_surface_listener
 * \{ */

#ifndef WITH_GHOST_WAYLAND_LIBDECOR

static void xdg_surface_handle_configure(void *data,
                                         xdg_surface *xdg_surface,
                                         const uint32_t serial)
{
  window_t *win = static_cast<window_t *>(data);

  if (win->xdg_surface != xdg_surface) {
    return;
  }

  if (win->size_pending[0] != 0 && win->size_pending[1] != 0) {
    win->size[0] = win->size_pending[0];
    win->size[1] = win->size_pending[1];
    wl_egl_window_resize(win->egl_window, win->size[0], win->size[1], 0, 0);
    win->size_pending[0] = 0;
    win->size_pending[1] = 0;
    win->w->notify_size();
  }

  if (win->is_active) {
    win->w->activate();
  }
  else {
    win->w->deactivate();
  }

  xdg_surface_ack_configure(xdg_surface, serial);
}

static const xdg_surface_listener xdg_surface_listener = {
    xdg_surface_handle_configure,
};

#endif /* !WITH_GHOST_WAYLAND_LIBDECOR. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Surface), #wl_surface_listener
 * \{ */

static void surface_handle_enter(void *data,
                                 struct wl_surface * /*wl_surface*/,
                                 struct wl_output *output)
{
  GHOST_WindowWayland *w = static_cast<GHOST_WindowWayland *>(data);
  output_t *reg_output = w->output_find_by_wl(output);
  if (reg_output == nullptr) {
    return;
  }

  if (w->outputs_enter(reg_output)) {
    w->outputs_changed_update_scale();
  }
}

static void surface_handle_leave(void *data,
                                 struct wl_surface * /*wl_surface*/,
                                 struct wl_output *output)
{
  GHOST_WindowWayland *w = static_cast<GHOST_WindowWayland *>(data);
  output_t *reg_output = w->output_find_by_wl(output);
  if (reg_output == nullptr) {
    return;
  }

  if (w->outputs_leave(reg_output)) {
    w->outputs_changed_update_scale();
  }
}

struct wl_surface_listener wl_surface_listener = {
    surface_handle_enter,
    surface_handle_leave,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ghost Implementation
 *
 * Wayland specific implementation of the GHOST_Window interface.
 * \{ */

GHOST_TSuccess GHOST_WindowWayland::hasCursorShape(GHOST_TStandardCursor cursorShape)
{
  return m_system->hasCursorShape(cursorShape);
}

GHOST_WindowWayland::GHOST_WindowWayland(GHOST_SystemWayland *system,
                                         const char *title,
                                         const int32_t /*left*/,
                                         const int32_t /*top*/,
                                         const uint32_t width,
                                         const uint32_t height,
                                         const GHOST_TWindowState state,
                                         const GHOST_IWindow *parentWindow,
                                         const GHOST_TDrawingContextType type,
                                         const bool is_dialog,
                                         const bool stereoVisual,
                                         const bool exclusive)
    : GHOST_Window(width, height, state, stereoVisual, exclusive),
      m_system(system),
      w(new window_t)
{
  w->w = this;

  w->size[0] = int32_t(width);
  w->size[1] = int32_t(height);

  w->is_dialog = is_dialog;

  /* NOTE(@campbellbarton): The scale set here to avoid flickering on startup.
   * When all monitors use the same scale (which is quite common) there aren't any problems.
   *
   * When monitors have different scales there may still be a visible window resize on startup.
   * Ideally it would be possible to know the scale this window will use however that's only
   * known once #surface_enter callback runs (which isn't guaranteed to run at all).
   *
   * Using the maximum scale is best as it results in the window first being smaller,
   * avoiding a large window flashing before it's made smaller. */
  w->scale = outputs_max_scale_or_default(this->m_system->outputs(), 1, &w->dpi);

  /* Window surfaces. */
  w->wl_surface = wl_compositor_create_surface(m_system->compositor());
  wl_surface_set_buffer_scale(this->surface(), w->scale);

  wl_surface_add_listener(w->wl_surface, &wl_surface_listener, this);

  w->egl_window = wl_egl_window_create(w->wl_surface, int(w->size[0]), int(w->size[1]));

  /* NOTE: The limit is in points (not pixels) so Hi-DPI will limit to larger number of pixels.
   * This has the advantage that the size limit is the same when moving the window between monitors
   * with different scales set. If it was important to limit in pixels it could be re-calculated
   * when the `w->scale` changed. */
  const int32_t size_min[2] = {320, 240};

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  /* create window decorations */
  w->decor_frame = libdecor_decorate(
      m_system->decor_context(), w->wl_surface, &libdecor_frame_iface, w);
  libdecor_frame_map(w->decor_frame);

  libdecor_frame_set_min_content_size(w->decor_frame, size_min[0], size_min[1]);

  if (parentWindow) {
    libdecor_frame_set_parent(
        w->decor_frame, dynamic_cast<const GHOST_WindowWayland *>(parentWindow)->w->decor_frame);
  }
#else
  w->xdg_surface = xdg_wm_base_get_xdg_surface(m_system->xdg_shell(), w->wl_surface);
  w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);

  xdg_toplevel_set_min_size(w->xdg_toplevel, size_min[0], size_min[1]);

  if (m_system->xdg_decoration_manager()) {
    w->xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
        m_system->xdg_decoration_manager(), w->xdg_toplevel);
    zxdg_toplevel_decoration_v1_add_listener(
        w->xdg_toplevel_decoration, &toplevel_decoration_v1_listener, w);
    zxdg_toplevel_decoration_v1_set_mode(w->xdg_toplevel_decoration,
                                         ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }

  xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);
  xdg_toplevel_add_listener(w->xdg_toplevel, &toplevel_listener, w);

  if (parentWindow && is_dialog) {
    xdg_toplevel_set_parent(
        w->xdg_toplevel, dynamic_cast<const GHOST_WindowWayland *>(parentWindow)->w->xdg_toplevel);
  }

#endif /* !WITH_GHOST_WAYLAND_LIBDECOR */

  setTitle(title);

  wl_surface_set_user_data(w->wl_surface, this);

  /* Call top-level callbacks. */
  wl_surface_commit(w->wl_surface);
  wl_display_roundtrip(m_system->display());

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  /* It's important not to return until the window is configured or
   * calls to `setState` from Blender will crash `libdecor`. */
  while (!w->decor_configured) {
    if (libdecor_dispatch(m_system->decor_context(), 0) < 0) {
      break;
    }
  }
#endif

#ifdef GHOST_OPENGL_ALPHA
  setOpaque();
#endif

#ifndef WITH_GHOST_WAYLAND_LIBDECOR /* Causes a glitch with `libdecor` for some reason. */
  setState(state);
#endif

  /* EGL context. */
  if (setDrawingContextType(type) == GHOST_kFailure) {
    GHOST_PRINT("Failed to create EGL context" << std::endl);
  }

  /* Set swap interval to 0 to prevent blocking. */
  setSwapInterval(0);
}

GHOST_TSuccess GHOST_WindowWayland::close()
{
  return m_system->pushEvent(
      new GHOST_Event(m_system->getMilliSeconds(), GHOST_kEventWindowClose, this));
}

GHOST_TSuccess GHOST_WindowWayland::activate()
{
  if (m_system->getWindowManager()->setActiveWindow(this) == GHOST_kFailure) {
    return GHOST_kFailure;
  }
  return m_system->pushEvent(
      new GHOST_Event(m_system->getMilliSeconds(), GHOST_kEventWindowActivate, this));
}

GHOST_TSuccess GHOST_WindowWayland::deactivate()
{
  m_system->getWindowManager()->setWindowInactive(this);
  return m_system->pushEvent(
      new GHOST_Event(m_system->getMilliSeconds(), GHOST_kEventWindowDeactivate, this));
}

GHOST_TSuccess GHOST_WindowWayland::notify_size()
{
#ifdef GHOST_OPENGL_ALPHA
  setOpaque();
#endif

  return m_system->pushEvent(
      new GHOST_Event(m_system->getMilliSeconds(), GHOST_kEventWindowSize, this));
}

wl_surface *GHOST_WindowWayland::surface() const
{
  return w->wl_surface;
}

const std::vector<output_t *> &GHOST_WindowWayland::outputs()
{
  return w->outputs;
}

output_t *GHOST_WindowWayland::output_find_by_wl(struct wl_output *output)
{
  for (output_t *reg_output : this->m_system->outputs()) {
    if (reg_output->wl_output == output) {
      return reg_output;
    }
  }
  return nullptr;
}

bool GHOST_WindowWayland::outputs_changed_update_scale()
{
  uint32_t dpi_next;
  const int scale_next = outputs_max_scale_or_default(this->outputs(), 0, &dpi_next);
  if (scale_next == 0) {
    return false;
  }

  window_t *win = this->w;
  const uint32_t dpi_curr = win->dpi;
  const int scale_curr = win->scale;
  bool changed = false;

  if (scale_next != scale_curr) {
    /* Unlikely but possible there is a pending size change is set. */
    win->size_pending[0] = (win->size_pending[0] / scale_curr) * scale_next;
    win->size_pending[1] = (win->size_pending[1] / scale_curr) * scale_next;

    win->scale = scale_next;
    wl_surface_set_buffer_scale(this->surface(), scale_next);
    changed = true;
  }

  if (dpi_next != dpi_curr) {
    /* Using the real DPI will cause wrong scaling of the UI
     * use a multiplier for the default DPI as workaround. */
    win->dpi = dpi_next;
    changed = true;
  }

  return changed;
}

bool GHOST_WindowWayland::outputs_enter(output_t *reg_output)
{
  std::vector<output_t *> &outputs = w->outputs;
  auto it = std::find(outputs.begin(), outputs.end(), reg_output);
  if (it != outputs.end()) {
    return false;
  }
  outputs.push_back(reg_output);
  return true;
}

bool GHOST_WindowWayland::outputs_leave(output_t *reg_output)
{
  std::vector<output_t *> &outputs = w->outputs;
  auto it = std::find(outputs.begin(), outputs.end(), reg_output);
  if (it == outputs.end()) {
    return false;
  }
  outputs.erase(it);
  return true;
}

uint16_t GHOST_WindowWayland::dpi()
{
  return w->dpi;
}

int GHOST_WindowWayland::scale()
{
  return w->scale;
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  return m_system->setCursorGrab(mode, m_cursorGrab, w->wl_surface);
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  const GHOST_TSuccess ok = m_system->setCursorShape(shape);
  m_cursorShape = (ok == GHOST_kSuccess) ? shape : GHOST_kStandardCursorDefault;
  return ok;
}

bool GHOST_WindowWayland::getCursorGrabUseSoftwareDisplay()
{
  return m_system->getCursorGrabUseSoftwareDisplay(m_cursorGrab);
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCustomCursorShape(
    uint8_t *bitmap, uint8_t *mask, int sizex, int sizey, int hotX, int hotY, bool canInvertColor)
{
  return m_system->setCustomCursorShape(bitmap, mask, sizex, sizey, hotX, hotY, canInvertColor);
}

GHOST_TSuccess GHOST_WindowWayland::getCursorBitmap(GHOST_CursorBitmapRef *bitmap)
{
  return m_system->getCursorBitmap(bitmap);
}

void GHOST_WindowWayland::setTitle(const char *title)
{
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor_frame_set_app_id(w->decor_frame, title);
  libdecor_frame_set_title(w->decor_frame, title);
#else
  xdg_toplevel_set_title(w->xdg_toplevel, title);
  xdg_toplevel_set_app_id(w->xdg_toplevel, title);
#endif

  this->title = title;
}

std::string GHOST_WindowWayland::getTitle() const
{
  return this->title.empty() ? "untitled" : this->title;
}

void GHOST_WindowWayland::getWindowBounds(GHOST_Rect &bounds) const
{
  getClientBounds(bounds);
}

void GHOST_WindowWayland::getClientBounds(GHOST_Rect &bounds) const
{
  bounds.set(0, 0, w->size[0], w->size[1]);
}

GHOST_TSuccess GHOST_WindowWayland::setClientWidth(const uint32_t width)
{
  return setClientSize(width, uint32_t(w->size[1]));
}

GHOST_TSuccess GHOST_WindowWayland::setClientHeight(const uint32_t height)
{
  return setClientSize(uint32_t(w->size[0]), height);
}

GHOST_TSuccess GHOST_WindowWayland::setClientSize(const uint32_t width, const uint32_t height)
{
  wl_egl_window_resize(w->egl_window, int(width), int(height), 0, 0);

  /* Override any pending size that may be set. */
  w->size_pending[0] = 0;
  w->size_pending[1] = 0;

  w->size[0] = width;
  w->size[1] = height;

  notify_size();

  return GHOST_kSuccess;
}

void GHOST_WindowWayland::screenToClient(int32_t inX,
                                         int32_t inY,
                                         int32_t &outX,
                                         int32_t &outY) const
{
  outX = inX;
  outY = inY;
}

void GHOST_WindowWayland::clientToScreen(int32_t inX,
                                         int32_t inY,
                                         int32_t &outX,
                                         int32_t &outY) const
{
  outX = inX;
  outY = inY;
}

GHOST_WindowWayland::~GHOST_WindowWayland()
{
  releaseNativeHandles();

  wl_egl_window_destroy(w->egl_window);

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor_frame_unref(w->decor_frame);
#else
  if (w->xdg_toplevel_decoration) {
    zxdg_toplevel_decoration_v1_destroy(w->xdg_toplevel_decoration);
  }
  xdg_toplevel_destroy(w->xdg_toplevel);
  xdg_surface_destroy(w->xdg_surface);
#endif

  wl_surface_destroy(w->wl_surface);

  /* NOTE(@campbellbarton): This is needed so the appropriate handlers event
   * (#wl_surface_listener.leave in particular) run to prevent access to the freed surfaces.
   * Without this round-trip, calling #getCursorPosition immediately after closing a window
   * causes dangling #wl_surface pointers to be accessed
   * (since the window is used for scaling the cursor position).
   *
   * An alternative solution would be to clear all internal pointers that reference this window.
   * Even though this is reasonable it introduces a 3rd state that needs to be accounted for,
   * where values are cleared before they have been set to their new values.
   * Any information requested in this state (such as the cursor position) won't be valid and
   * could cause difficult to reproduce bugs. So perform a round-trip as closing a window isn't
   * an action that runs continuously & isn't likely to cause unnecessary overhead. See: T99078. */
  wl_display_roundtrip(m_system->display());

  delete w;
}

uint16_t GHOST_WindowWayland::getDPIHint()
{
  return w->dpi;
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCursorVisibility(bool visible)
{
  return m_system->setCursorVisibility(visible);
}

GHOST_TSuccess GHOST_WindowWayland::setState(GHOST_TWindowState state)
{
  switch (state) {
    case GHOST_kWindowStateNormal:
      /* Unset states. */
      switch (getState()) {
        case GHOST_kWindowStateMaximized:
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
          libdecor_frame_unset_maximized(w->decor_frame);
#else
          xdg_toplevel_unset_maximized(w->xdg_toplevel);
#endif
          break;
        case GHOST_kWindowStateFullScreen:
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
          libdecor_frame_unset_fullscreen(w->decor_frame);
#else
          xdg_toplevel_unset_fullscreen(w->xdg_toplevel);
#endif
          break;
        default:
          break;
      }
      break;
    case GHOST_kWindowStateMaximized:
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
      libdecor_frame_set_maximized(w->decor_frame);
#else
      xdg_toplevel_set_maximized(w->xdg_toplevel);
#endif
      break;
    case GHOST_kWindowStateMinimized:
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
      libdecor_frame_set_minimized(w->decor_frame);
#else
      xdg_toplevel_set_minimized(w->xdg_toplevel);
#endif
      break;
    case GHOST_kWindowStateFullScreen:
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
      libdecor_frame_set_fullscreen(w->decor_frame, nullptr);
#else
      xdg_toplevel_set_fullscreen(w->xdg_toplevel, nullptr);
#endif
      break;
    case GHOST_kWindowStateEmbedded:
      return GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_TWindowState GHOST_WindowWayland::getState() const
{
  if (w->is_fullscreen) {
    return GHOST_kWindowStateFullScreen;
  }
  if (w->is_maximised) {
    return GHOST_kWindowStateMaximized;
  }
  return GHOST_kWindowStateNormal;
}

GHOST_TSuccess GHOST_WindowWayland::invalidate()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWayland::setOrder(GHOST_TWindowOrder /*order*/)
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWayland::beginFullScreen() const
{
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor_frame_set_fullscreen(w->decor_frame, nullptr);
#else
  xdg_toplevel_set_fullscreen(w->xdg_toplevel, nullptr);
#endif
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWayland::endFullScreen() const
{
#ifdef WITH_GHOST_WAYLAND_LIBDECOR
  libdecor_frame_unset_fullscreen(w->decor_frame);
#else
  xdg_toplevel_unset_fullscreen(w->xdg_toplevel);
#endif
  return GHOST_kSuccess;
}

bool GHOST_WindowWayland::isDialog() const
{
  return w->is_dialog;
}

#ifdef GHOST_OPENGL_ALPHA
void GHOST_WindowWayland::setOpaque() const
{
  struct wl_region *region;

  /* Make the window opaque. */
  region = wl_compositor_create_region(m_system->compositor());
  wl_region_add(region, 0, 0, w->size[0], w->size[1]);
  wl_surface_set_opaque_region(w->surface, region);
  wl_region_destroy(region);
}
#endif

/**
 * \param type: The type of rendering context create.
 * \return Indication of success.
 */
GHOST_Context *GHOST_WindowWayland::newDrawingContext(GHOST_TDrawingContextType type)
{
  GHOST_Context *context;
  switch (type) {
    case GHOST_kDrawingContextTypeNone:
      context = new GHOST_ContextNone(m_wantStereoVisual);
      break;
    case GHOST_kDrawingContextTypeOpenGL:
      for (int minor = 6; minor >= 0; --minor) {
        context = new GHOST_ContextEGL(this->m_system,
                                       m_wantStereoVisual,
                                       EGLNativeWindowType(w->egl_window),
                                       EGLNativeDisplayType(m_system->display()),
                                       EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                       4,
                                       minor,
                                       GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                       GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                       EGL_OPENGL_API);

        if (context->initializeDrawingContext()) {
          return context;
        }
        delete context;
      }
      context = new GHOST_ContextEGL(this->m_system,
                                     m_wantStereoVisual,
                                     EGLNativeWindowType(w->egl_window),
                                     EGLNativeDisplayType(m_system->display()),
                                     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                     3,
                                     3,
                                     GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                     GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                     EGL_OPENGL_API);
  }

  return (context->initializeDrawingContext() == GHOST_kSuccess) ? context : nullptr;
}

/** \} */
