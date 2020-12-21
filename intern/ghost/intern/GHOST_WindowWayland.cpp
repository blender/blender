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

#include "GHOST_WindowWayland.h"
#include "GHOST_SystemWayland.h"
#include "GHOST_WindowManager.h"

#include "GHOST_Event.h"

#include "GHOST_ContextEGL.h"
#include "GHOST_ContextNone.h"

#include <wayland-egl.h>

struct window_t {
  GHOST_WindowWayland *w;
  wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  wl_egl_window *egl_window;
  int32_t pending_width, pending_height;
  bool is_maximised;
  bool is_fullscreen;
  bool is_active;
  bool is_dialog;
  int32_t width, height;
};

/* -------------------------------------------------------------------- */
/** \name Wayland Interface Callbacks
 *
 * These callbacks are registered for Wayland interfaces and called when
 * an event is received from the compositor.
 * \{ */

static void toplevel_configure(
    void *data, xdg_toplevel * /*xdg_toplevel*/, int32_t width, int32_t height, wl_array *states)
{
  window_t *win = static_cast<window_t *>(data);
  win->pending_width = width;
  win->pending_height = height;

  win->is_maximised = false;
  win->is_fullscreen = false;
  win->is_active = false;

  /* Note that the macro 'wl_array_for_each' would typically be used to simplify this logic,
   * however it's not compatible with C++, so perform casts instead.
   * If this needs to be done more often we could define our own C++ compatible macro. */
  for (enum xdg_toplevel_state *state = static_cast<xdg_toplevel_state *>(states->data);
       reinterpret_cast<uint8_t *>(state) < (static_cast<uint8_t *>(states->data) + states->size);
       state++) {
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

static void toplevel_close(void *data, xdg_toplevel * /*xdg_toplevel*/)
{
  static_cast<window_t *>(data)->w->close();
}

static const xdg_toplevel_listener toplevel_listener = {
    toplevel_configure,
    toplevel_close,
};

static void surface_configure(void *data, xdg_surface *xdg_surface, uint32_t serial)
{
  window_t *win = static_cast<window_t *>(data);

  int w, h;
  wl_egl_window_get_attached_size(win->egl_window, &w, &h);
  if (win->pending_width != 0 && win->pending_height != 0 && win->pending_width != w &&
      win->pending_height != h) {
    win->width = win->pending_width;
    win->height = win->pending_height;
    wl_egl_window_resize(win->egl_window, win->pending_width, win->pending_height, 0, 0);
    win->pending_width = 0;
    win->pending_height = 0;
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

static const xdg_surface_listener surface_listener = {
    surface_configure,
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
                                         GHOST_TInt32 /*left*/,
                                         GHOST_TInt32 /*top*/,
                                         GHOST_TUns32 width,
                                         GHOST_TUns32 height,
                                         GHOST_TWindowState state,
                                         const GHOST_IWindow *parentWindow,
                                         GHOST_TDrawingContextType type,
                                         const bool is_dialog,
                                         const bool stereoVisual,
                                         const bool exclusive)
    : GHOST_Window(width, height, state, stereoVisual, exclusive),
      m_system(system),
      w(new window_t)
{
  w->w = this;

  w->width = int32_t(width);
  w->height = int32_t(height);

  w->is_dialog = is_dialog;

  /* Window surfaces. */
  w->surface = wl_compositor_create_surface(m_system->compositor());
  w->egl_window = wl_egl_window_create(w->surface, int(width), int(height));

  w->xdg_surface = xdg_wm_base_get_xdg_surface(m_system->shell(), w->surface);
  w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);

  wl_surface_set_user_data(w->surface, this);

  xdg_surface_add_listener(w->xdg_surface, &surface_listener, w);
  xdg_toplevel_add_listener(w->xdg_toplevel, &toplevel_listener, w);

  if (parentWindow) {
    xdg_toplevel_set_parent(
        w->xdg_toplevel, dynamic_cast<const GHOST_WindowWayland *>(parentWindow)->w->xdg_toplevel);
  }

  /* Call top-level callbacks. */
  wl_surface_commit(w->surface);
  wl_display_roundtrip(m_system->display());

#ifdef GHOST_OPENGL_ALPHA
  setOpaque();
#endif

  setState(state);

  setTitle(title);

  /* EGL context. */
  if (setDrawingContextType(type) == GHOST_kFailure) {
    GHOST_PRINT("Failed to create EGL context" << std::endl);
  }
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

GHOST_TSuccess GHOST_WindowWayland::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  return m_system->setCursorGrab(mode, w->surface);
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  const GHOST_TSuccess ok = m_system->setCursorShape(shape);
  m_cursorShape = (ok == GHOST_kSuccess) ? shape : GHOST_kStandardCursorDefault;
  return ok;
}

GHOST_TSuccess GHOST_WindowWayland::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                                               GHOST_TUns8 *mask,
                                                               int sizex,
                                                               int sizey,
                                                               int hotX,
                                                               int hotY,
                                                               bool canInvertColor)
{
  return m_system->setCustomCursorShape(bitmap, mask, sizex, sizey, hotX, hotY, canInvertColor);
}

void GHOST_WindowWayland::setTitle(const char *title)
{
  xdg_toplevel_set_title(w->xdg_toplevel, title);
  xdg_toplevel_set_app_id(w->xdg_toplevel, title);
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
  bounds.set(0, 0, w->width, w->height);
}

GHOST_TSuccess GHOST_WindowWayland::setClientWidth(GHOST_TUns32 width)
{
  return setClientSize(width, GHOST_TUns32(w->height));
}

GHOST_TSuccess GHOST_WindowWayland::setClientHeight(GHOST_TUns32 height)
{
  return setClientSize(GHOST_TUns32(w->width), height);
}

GHOST_TSuccess GHOST_WindowWayland::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
  wl_egl_window_resize(w->egl_window, int(width), int(height), 0, 0);
  return GHOST_kSuccess;
}

void GHOST_WindowWayland::screenToClient(GHOST_TInt32 inX,
                                         GHOST_TInt32 inY,
                                         GHOST_TInt32 &outX,
                                         GHOST_TInt32 &outY) const
{
  outX = inX;
  outY = inY;
}

void GHOST_WindowWayland::clientToScreen(GHOST_TInt32 inX,
                                         GHOST_TInt32 inY,
                                         GHOST_TInt32 &outX,
                                         GHOST_TInt32 &outY) const
{
  outX = inX;
  outY = inY;
}

GHOST_WindowWayland::~GHOST_WindowWayland()
{
  releaseNativeHandles();

  wl_egl_window_destroy(w->egl_window);
  xdg_toplevel_destroy(w->xdg_toplevel);
  xdg_surface_destroy(w->xdg_surface);
  wl_surface_destroy(w->surface);

  delete w;
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
          xdg_toplevel_unset_maximized(w->xdg_toplevel);
          break;
        case GHOST_kWindowStateFullScreen:
          xdg_toplevel_unset_fullscreen(w->xdg_toplevel);
          break;
        default:
          break;
      }
      break;
    case GHOST_kWindowStateMaximized:
      xdg_toplevel_set_maximized(w->xdg_toplevel);
      break;
    case GHOST_kWindowStateMinimized:
      xdg_toplevel_set_minimized(w->xdg_toplevel);
      break;
    case GHOST_kWindowStateFullScreen:
      xdg_toplevel_set_fullscreen(w->xdg_toplevel, nullptr);
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
  else if (w->is_maximised) {
    return GHOST_kWindowStateMaximized;
  }
  else {
    return GHOST_kWindowStateNormal;
  }
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
  xdg_toplevel_set_fullscreen(w->xdg_toplevel, nullptr);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWayland::endFullScreen() const
{
  xdg_toplevel_unset_fullscreen(w->xdg_toplevel);
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
  wl_region_add(region, 0, 0, w->width, w->height);
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
      context = new GHOST_ContextEGL(m_wantStereoVisual,
                                     EGLNativeWindowType(w->egl_window),
                                     EGLNativeDisplayType(m_system->display()),
                                     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                     3,
                                     3,
                                     GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                     GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                     EGL_OPENGL_API);
      break;
  }

  return (context->initializeDrawingContext() == GHOST_kSuccess) ? context : nullptr;
}

/** \} */
