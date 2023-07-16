/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/* For standard X11 cursors */
#include <X11/Xatom.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include "GHOST_Debug.hh"
#include "GHOST_IconX11.hh"
#include "GHOST_SystemX11.hh"
#include "GHOST_Types.h"
#include "GHOST_WindowX11.hh"
#include "GHOST_utildefines.hh"

#ifdef WITH_XDND
#  include "GHOST_DropTargetX11.hh"
#endif

#ifdef WITH_OPENGL_BACKEND
#  include "GHOST_ContextEGL.hh"
#  include "GHOST_ContextGLX.hh"
#endif
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

/* For #XIWarpPointer. */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput2.h>
#endif

/* For DPI value. */
#include <X11/Xresource.h>

#include <cstdio>
#include <cstring>

/* For `gethostname`. */
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <string>

/* For obscure full screen mode stuff
 * lifted verbatim from blut. */

using MotifWmHints = struct {
  long flags;
  long functions;
  long decorations;
  long input_mode;
};

enum {
  MWM_HINTS_FUNCTIONS = (1L << 0),
  MWM_HINTS_DECORATIONS = (1L << 1),
};
enum {
  MWM_FUNCTION_ALL = (1L << 0),
  MWM_FUNCTION_RESIZE = (1L << 1),
  MWM_FUNCTION_MOVE = (1L << 2),
  MWM_FUNCTION_MINIMIZE = (1L << 3),
  MWM_FUNCTION_MAXIMIZE = (1L << 4),
  MWM_FUNCTION_CLOSE = (1L << 5),
};

#ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX 64
#endif

// #define GHOST_X11_GRAB

/*
 * A Client can't change the window property, that is
 * the work of the window manager. In case, we send
 * a ClientMessage to the RootWindow with the property
 * and the Action (WM-spec define this):
 */
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
// #define _NET_WM_STATE_TOGGLE 2 // UNUSED

#ifdef WITH_OPENGL_BACKEND
static XVisualInfo *get_x11_visualinfo(Display *display)
{
  int num_visuals;
  XVisualInfo vinfo_template;
  vinfo_template.screen = DefaultScreen(display);
  return XGetVisualInfo(display, VisualScreenMask, &vinfo_template, &num_visuals);
}
#endif

GHOST_WindowX11::GHOST_WindowX11(GHOST_SystemX11 *system,
                                 Display *display,
                                 const char *title,
                                 int32_t left,
                                 int32_t top,
                                 uint32_t width,
                                 uint32_t height,
                                 GHOST_TWindowState state,
                                 GHOST_WindowX11 *parentWindow,
                                 GHOST_TDrawingContextType type,
                                 const bool is_dialog,
                                 const bool stereoVisual,
                                 const bool exclusive,
                                 const bool is_debug)
    : GHOST_Window(width, height, state, stereoVisual, exclusive),
      m_display(display),
      m_visualInfo(nullptr),
      m_fbconfig(nullptr),
      m_normal_state(GHOST_kWindowStateNormal),
      m_system(system),
      m_invalid_window(false),
      m_empty_cursor(None),
      m_custom_cursor(None),
      m_visible_cursor(None),
      m_taskbar("blender.desktop"),
#ifdef WITH_XDND
      m_dropTarget(nullptr),
#endif
      m_tabletData(GHOST_TABLET_DATA_NONE),
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      m_xic(nullptr),
#endif
      m_valid_setup(false),
      m_is_debug_context(is_debug)
{
#ifdef WITH_OPENGL_BACKEND
  if (type == GHOST_kDrawingContextTypeOpenGL) {
    m_visualInfo = get_x11_visualinfo(m_display);
  }
  else
#endif
  {
    XVisualInfo tmp = {nullptr};
    int n;
    m_visualInfo = XGetVisualInfo(m_display, 0, &tmp, &n);
  }

  /* caller needs to check 'getValid()' */
  if (m_visualInfo == nullptr) {
    fprintf(stderr, "initial window could not find the GLX extension\n");
    return;
  }

  uint xattributes_valuemask = 0;

  XSetWindowAttributes xattributes;
  memset(&xattributes, 0, sizeof(xattributes));

  xattributes_valuemask |= CWBorderPixel;
  xattributes.border_pixel = 0;

  /* Specify which events we are interested in hearing. */

  xattributes_valuemask |= CWEventMask;
  xattributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                           EnterWindowMask | LeaveWindowMask | ButtonPressMask |
                           ButtonReleaseMask | PointerMotionMask | FocusChangeMask |
                           PropertyChangeMask | KeymapStateMask;

  if (exclusive) {
    xattributes_valuemask |= CWOverrideRedirect;
    xattributes.override_redirect = True;
  }

  xattributes_valuemask |= CWColormap;
  xattributes.colormap = XCreateColormap(
      m_display, RootWindow(m_display, m_visualInfo->screen), m_visualInfo->visual, AllocNone);

  /* create the window! */
  m_window = XCreateWindow(m_display,
                           RootWindow(m_display, m_visualInfo->screen),
                           left,
                           top,
                           width,
                           height,
                           0, /* no border. */
                           m_visualInfo->depth,
                           InputOutput,
                           m_visualInfo->visual,
                           xattributes_valuemask,
                           &xattributes);

#ifdef WITH_XDND
  /* initialize drop target for newly created window */
  m_dropTarget = new GHOST_DropTargetX11(this, m_system);
  GHOST_PRINT("Set drop target\n");
#endif

  if (ELEM(state, GHOST_kWindowStateMaximized, GHOST_kWindowStateFullScreen)) {
    Atom atoms[2];
    int count = 0;
    if (state == GHOST_kWindowStateMaximized) {
      atoms[count++] = m_system->m_atom._NET_WM_STATE_MAXIMIZED_VERT;
      atoms[count++] = m_system->m_atom._NET_WM_STATE_MAXIMIZED_HORZ;
    }
    else {
      atoms[count++] = m_system->m_atom._NET_WM_STATE_FULLSCREEN;
    }

    XChangeProperty(m_display,
                    m_window,
                    m_system->m_atom._NET_WM_STATE,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (uchar *)atoms,
                    count);
    m_post_init = False;
  }
  /*
   * One of the problem with WM-spec is that can't set a property
   * to a window that isn't mapped. That is why we can't "just
   * call setState" here.
   *
   * To fix this, we first need know that the window is really
   * map waiting for the MapNotify event.
   *
   * So, m_post_init indicate that we need wait for the MapNotify
   * event and then set the Window state to the m_post_state.
   */
  else if (!ELEM(state, GHOST_kWindowStateNormal, GHOST_kWindowStateMinimized)) {
    m_post_init = True;
    m_post_state = state;
  }
  else {
    m_post_init = False;
    m_post_state = GHOST_kWindowStateNormal;
  }

  if (is_dialog && parentWindow) {
    setDialogHints(parentWindow);
  }

  /* Create some hints for the window manager on how
   * we want this window treated. */
  {
    XSizeHints *xsizehints = XAllocSizeHints();
    xsizehints->flags = PPosition | PSize | PMinSize | PMaxSize;
    xsizehints->x = left;
    xsizehints->y = top;
    xsizehints->width = width;
    xsizehints->height = height;
    xsizehints->min_width = 320;  /* size hints, could be made apart of the ghost api */
    xsizehints->min_height = 240; /* limits are also arbitrary, but should not allow 1x1 window */
    xsizehints->max_width = 65535;
    xsizehints->max_height = 65535;
    XSetWMNormalHints(m_display, m_window, xsizehints);
    XFree(xsizehints);
  }

  /* XClassHint, title */
  {
    XClassHint *xclasshint = XAllocClassHint();
    const int len = strlen(title) + 1;
    char *wmclass = (char *)malloc(sizeof(char) * len);
    memcpy(wmclass, title, len * sizeof(char));
    xclasshint->res_name = wmclass;
    xclasshint->res_class = wmclass;
    XSetClassHint(m_display, m_window, xclasshint);
    free(wmclass);
    XFree(xclasshint);
  }

  /* The basic for a good ICCCM "work" */
  if (m_system->m_atom.WM_PROTOCOLS) {
    Atom atoms[2];
    int natom = 0;

    if (m_system->m_atom.WM_DELETE_WINDOW) {
      atoms[natom] = m_system->m_atom.WM_DELETE_WINDOW;
      natom++;
    }

    if (m_system->m_atom.WM_TAKE_FOCUS && m_system->m_windowFocus) {
      atoms[natom] = m_system->m_atom.WM_TAKE_FOCUS;
      natom++;
    }

    if (natom) {
      // printf("Register atoms: %d\n", natom);
      XSetWMProtocols(m_display, m_window, atoms, natom);
    }
  }

  /* Set the window hints */
  {
    XWMHints *xwmhints = XAllocWMHints();
    xwmhints->initial_state = NormalState;
    xwmhints->input = (m_system->m_windowFocus) ? True : False;
    xwmhints->flags = InputHint | StateHint;
    XSetWMHints(display, m_window, xwmhints);
    XFree(xwmhints);
  }

  /* set the icon */
  {
    Atom _NET_WM_ICON = XInternAtom(m_display, "_NET_WM_ICON", False);
    XChangeProperty(m_display,
                    m_window,
                    _NET_WM_ICON,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (uchar *)BLENDER_ICONS_WM_X11,
                    ARRAY_SIZE(BLENDER_ICONS_WM_X11));
  }

  /* set the process ID (_NET_WM_PID) */
  {
    Atom _NET_WM_PID = XInternAtom(m_display, "_NET_WM_PID", False);
    pid_t pid = getpid();
    XChangeProperty(
        m_display, m_window, _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (uchar *)&pid, 1);
  }

  /* set the hostname (WM_CLIENT_MACHINE) */
  {
    char hostname[HOST_NAME_MAX];
    char *text_array[1];
    XTextProperty text_prop;

    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0';
    text_array[0] = hostname;

    XStringListToTextProperty(text_array, 1, &text_prop);
    XSetWMClientMachine(m_display, m_window, &text_prop);
    XFree(text_prop.value);
  }

#ifdef WITH_X11_XINPUT
  refreshXInputDevices();
#endif

  /* now set up the rendering context. */
  if (setDrawingContextType(type) == GHOST_kSuccess) {
    m_valid_setup = true;
    GHOST_PRINT("Created window\n");
  }

  setTitle(title);

  if (exclusive && system->m_windowFocus) {
    XMapRaised(m_display, m_window);
  }
  else {
    XMapWindow(m_display, m_window);

    if (!system->m_windowFocus) {
      XLowerWindow(m_display, m_window);
    }
  }
  GHOST_PRINT("Mapped window\n");

  XFlush(m_display);
}

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
static Bool destroyICCallback(XIC /*xic*/, XPointer ptr, XPointer /*data*/)
{
  GHOST_PRINT("XIM input context destroyed\n");

  if (ptr) {
    *(XIC *)ptr = nullptr;
  }
  /* Ignored by X11. */
  return True;
}

bool GHOST_WindowX11::createX11_XIC()
{
  XIM xim = m_system->getX11_XIM();
  if (!xim) {
    return false;
  }

  XICCallback destroy;
  destroy.callback = (XICProc)destroyICCallback;
  destroy.client_data = (XPointer)&m_xic;
  m_xic = XCreateIC(xim,
                    XNClientWindow,
                    m_window,
                    XNFocusWindow,
                    m_window,
                    XNInputStyle,
                    XIMPreeditNothing | XIMStatusNothing,
                    XNResourceName,
                    GHOST_X11_RES_NAME,
                    XNResourceClass,
                    GHOST_X11_RES_CLASS,
                    XNDestroyCallback,
                    &destroy,
                    nullptr);
  if (!m_xic)
    return false;

  ulong fevent;
  XGetICValues(m_xic, XNFilterEvents, &fevent, nullptr);
  XSelectInput(m_display,
               m_window,
               ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                   EnterWindowMask | LeaveWindowMask | ButtonPressMask | ButtonReleaseMask |
                   PointerMotionMask | FocusChangeMask | PropertyChangeMask | KeymapStateMask |
                   fevent);
  return true;
}
#endif

#ifdef WITH_X11_XINPUT
void GHOST_WindowX11::refreshXInputDevices()
{
  if (m_system->m_xinput_version.present) {
    std::vector<XEventClass> xevents;

    for (GHOST_SystemX11::GHOST_TabletX11 &xtablet : m_system->GetXTablets()) {
      /* With modern XInput (XLIB 1.6.2 at least and/or EVDEV 2.9.0) and some 'no-name' tablets
       * like 'UC-LOGIC Tablet WP5540U', we also need to 'select' ButtonPress for motion event,
       * otherwise we do not get any tablet motion event once pen is pressed... See #43367.
       */
      XEventClass ev;

      DeviceMotionNotify(xtablet.Device, xtablet.MotionEvent, ev);
      if (ev) {
        xevents.push_back(ev);
      }
      DeviceButtonPress(xtablet.Device, xtablet.PressEvent, ev);
      if (ev) {
        xevents.push_back(ev);
      }
      ProximityIn(xtablet.Device, xtablet.ProxInEvent, ev);
      if (ev) {
        xevents.push_back(ev);
      }
      ProximityOut(xtablet.Device, xtablet.ProxOutEvent, ev);
      if (ev) {
        xevents.push_back(ev);
      }
    }

    XSelectExtensionEvent(m_display, m_window, xevents.data(), int(xevents.size()));
  }
}

#endif /* WITH_X11_XINPUT */

Window GHOST_WindowX11::getXWindow()
{
  return m_window;
}

bool GHOST_WindowX11::getValid() const
{
  return GHOST_Window::getValid() && m_valid_setup;
}

void GHOST_WindowX11::setTitle(const char *title)
{
  Atom name = XInternAtom(m_display, "_NET_WM_NAME", 0);
  Atom utf8str = XInternAtom(m_display, "UTF8_STRING", 0);
  XChangeProperty(
      m_display, m_window, name, utf8str, 8, PropModeReplace, (const uchar *)title, strlen(title));

  /* This should convert to valid x11 string
   * and getTitle would need matching change */
  XStoreName(m_display, m_window, title);

  XFlush(m_display);
}

std::string GHOST_WindowX11::getTitle() const
{
  char *name = nullptr;

  XFetchName(m_display, m_window, &name);
  std::string title = name ? name : "untitled";
  XFree(name);
  return title;
}

void GHOST_WindowX11::getWindowBounds(GHOST_Rect &bounds) const
{
  /* Getting the window bounds under X11 is not
   * really supported (nor should it be desired). */
  getClientBounds(bounds);
}

void GHOST_WindowX11::getClientBounds(GHOST_Rect &bounds) const
{
  Window root_return;
  int x_return, y_return;
  uint w_return, h_return, border_w_return, depth_return;
  int32_t screen_x, screen_y;

  XGetGeometry(m_display,
               m_window,
               &root_return,
               &x_return,
               &y_return,
               &w_return,
               &h_return,
               &border_w_return,
               &depth_return);

  clientToScreen(0, 0, screen_x, screen_y);

  bounds.m_l = screen_x;
  bounds.m_r = bounds.m_l + w_return;
  bounds.m_t = screen_y;
  bounds.m_b = bounds.m_t + h_return;
}

GHOST_TSuccess GHOST_WindowX11::setClientWidth(uint32_t width)
{
  XWindowChanges values;
  uint value_mask = CWWidth;
  values.width = width;
  XConfigureWindow(m_display, m_window, value_mask, &values);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setClientHeight(uint32_t height)
{
  XWindowChanges values;
  uint value_mask = CWHeight;
  values.height = height;
  XConfigureWindow(m_display, m_window, value_mask, &values);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setClientSize(uint32_t width, uint32_t height)
{
  XWindowChanges values;
  uint value_mask = CWWidth | CWHeight;
  values.width = width;
  values.height = height;
  XConfigureWindow(m_display, m_window, value_mask, &values);
  return GHOST_kSuccess;
}

void GHOST_WindowX11::screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const
{
  int ax, ay;
  Window temp;

  /* Use (0, 0) instead of (inX, inY) to work around overflow of signed int16 in
   * the implementation of this function. */
  XTranslateCoordinates(
      m_display, RootWindow(m_display, m_visualInfo->screen), m_window, 0, 0, &ax, &ay, &temp);
  outX = ax + inX;
  outY = ay + inY;
}

void GHOST_WindowX11::clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const
{
  int ax, ay;
  Window temp;

  XTranslateCoordinates(
      m_display, m_window, RootWindow(m_display, m_visualInfo->screen), inX, inY, &ax, &ay, &temp);
  outX = ax;
  outY = ay;
}

GHOST_TSuccess GHOST_WindowX11::setDialogHints(GHOST_WindowX11 *parentWindow)
{

  Atom atom_window_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
  Atom atom_dialog = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  MotifWmHints hints = {0};

  XChangeProperty(m_display,
                  m_window,
                  atom_window_type,
                  XA_ATOM,
                  32,
                  PropModeReplace,
                  (uchar *)&atom_dialog,
                  1);
  XSetTransientForHint(m_display, m_window, parentWindow->m_window);

  /* Disable minimizing of the window for now.
   * Actually, most window managers disable minimizing and maximizing for dialogs, ignoring this.
   * Leaving it here anyway in the hope it brings back maximizing on some window managers at least,
   * we'd preferably have it even for dialog windows (e.g. file browser). */
  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = MWM_FUNCTION_RESIZE | MWM_FUNCTION_MOVE | MWM_FUNCTION_MAXIMIZE |
                    MWM_FUNCTION_CLOSE;
  XChangeProperty(m_display,
                  m_window,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  32,
                  PropModeReplace,
                  (uchar *)&hints,
                  4);

  return GHOST_kSuccess;
}

void GHOST_WindowX11::icccmSetState(int state)
{
  XEvent xev;

  if (state != IconicState) {
    return;
  }

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = m_display;
  xev.xclient.window = m_window;
  xev.xclient.format = 32;
  xev.xclient.message_type = m_system->m_atom.WM_CHANGE_STATE;
  xev.xclient.data.l[0] = state;
  XSendEvent(m_display,
             RootWindow(m_display, m_visualInfo->screen),
             False,
             SubstructureNotifyMask | SubstructureRedirectMask,
             &xev);
}

int GHOST_WindowX11::icccmGetState() const
{
  struct {
    CARD32 state;
    XID icon;
  } * prop_ret;
  ulong bytes_after, num_ret;
  Atom type_ret;
  int ret, format_ret;
  CARD32 st;

  prop_ret = nullptr;
  ret = XGetWindowProperty(m_display,
                           m_window,
                           m_system->m_atom.WM_STATE,
                           0,
                           2,
                           False,
                           m_system->m_atom.WM_STATE,
                           &type_ret,
                           &format_ret,
                           &num_ret,
                           &bytes_after,
                           ((uchar **)&prop_ret));
  if ((ret == Success) && (prop_ret != nullptr) && (num_ret == 2)) {
    st = prop_ret->state;
  }
  else {
    st = NormalState;
  }

  if (prop_ret) {
    XFree(prop_ret);
  }

  return st;
}

void GHOST_WindowX11::netwmMaximized(bool set)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.window = m_window;
  xev.xclient.message_type = m_system->m_atom._NET_WM_STATE;
  xev.xclient.format = 32;

  if (set == True) {
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
  }
  else {
    xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
  }

  xev.xclient.data.l[1] = m_system->m_atom._NET_WM_STATE_MAXIMIZED_HORZ;
  xev.xclient.data.l[2] = m_system->m_atom._NET_WM_STATE_MAXIMIZED_VERT;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  XSendEvent(m_display,
             RootWindow(m_display, m_visualInfo->screen),
             False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &xev);
}

bool GHOST_WindowX11::netwmIsMaximized() const
{
  Atom *prop_ret;
  ulong bytes_after, num_ret, i;
  Atom type_ret;
  bool st;
  int format_ret, ret, count;

  prop_ret = nullptr;
  st = False;
  ret = XGetWindowProperty(m_display,
                           m_window,
                           m_system->m_atom._NET_WM_STATE,
                           0,
                           INT_MAX,
                           False,
                           XA_ATOM,
                           &type_ret,
                           &format_ret,
                           &num_ret,
                           &bytes_after,
                           (uchar **)&prop_ret);
  if ((ret == Success) && (prop_ret) && (format_ret == 32)) {
    count = 0;
    for (i = 0; i < num_ret; i++) {
      if (prop_ret[i] == m_system->m_atom._NET_WM_STATE_MAXIMIZED_HORZ) {
        count++;
      }
      if (prop_ret[i] == m_system->m_atom._NET_WM_STATE_MAXIMIZED_VERT) {
        count++;
      }
      if (count == 2) {
        st = True;
        break;
      }
    }
  }

  if (prop_ret) {
    XFree(prop_ret);
  }
  return st;
}

void GHOST_WindowX11::netwmFullScreen(bool set)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.window = m_window;
  xev.xclient.message_type = m_system->m_atom._NET_WM_STATE;
  xev.xclient.format = 32;

  if (set == True) {
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
  }
  else {
    xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
  }

  xev.xclient.data.l[1] = m_system->m_atom._NET_WM_STATE_FULLSCREEN;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  XSendEvent(m_display,
             RootWindow(m_display, m_visualInfo->screen),
             False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &xev);
}

bool GHOST_WindowX11::netwmIsFullScreen() const
{
  Atom *prop_ret;
  ulong bytes_after, num_ret, i;
  Atom type_ret;
  bool st;
  int format_ret, ret;

  prop_ret = nullptr;
  st = False;
  ret = XGetWindowProperty(m_display,
                           m_window,
                           m_system->m_atom._NET_WM_STATE,
                           0,
                           INT_MAX,
                           False,
                           XA_ATOM,
                           &type_ret,
                           &format_ret,
                           &num_ret,
                           &bytes_after,
                           (uchar **)&prop_ret);
  if ((ret == Success) && (prop_ret) && (format_ret == 32)) {
    for (i = 0; i < num_ret; i++) {
      if (prop_ret[i] == m_system->m_atom._NET_WM_STATE_FULLSCREEN) {
        st = True;
        break;
      }
    }
  }

  if (prop_ret) {
    XFree(prop_ret);
  }
  return st;
}

void GHOST_WindowX11::motifFullScreen(bool set)
{
  MotifWmHints hints;

  hints.flags = MWM_HINTS_DECORATIONS;
  if (set == True) {
    hints.decorations = 0;
  }
  else {
    hints.decorations = 1;
  }

  XChangeProperty(m_display,
                  m_window,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  32,
                  PropModeReplace,
                  (uchar *)&hints,
                  4);
}

bool GHOST_WindowX11::motifIsFullScreen() const
{
  MotifWmHints *prop_ret;
  ulong bytes_after, num_ret;
  Atom type_ret;
  bool state;
  int format_ret, st;

  prop_ret = nullptr;
  state = False;
  st = XGetWindowProperty(m_display,
                          m_window,
                          m_system->m_atom._MOTIF_WM_HINTS,
                          0,
                          INT_MAX,
                          False,
                          m_system->m_atom._MOTIF_WM_HINTS,
                          &type_ret,
                          &format_ret,
                          &num_ret,
                          &bytes_after,
                          (uchar **)&prop_ret);
  if ((st == Success) && prop_ret) {
    if (prop_ret->flags & MWM_HINTS_DECORATIONS) {
      if (!prop_ret->decorations) {
        state = True;
      }
    }
  }

  if (prop_ret) {
    XFree(prop_ret);
  }
  return state;
}

GHOST_TWindowState GHOST_WindowX11::getState() const
{
  GHOST_TWindowState state_ret;
  int state;

  state_ret = GHOST_kWindowStateNormal;
  state = icccmGetState();
  /*
   * In the Iconic and Withdrawn state, the window
   * is unmapped, so only need return a Minimized state.
   */
  if (ELEM(state, IconicState, WithdrawnState)) {
    state_ret = GHOST_kWindowStateMinimized;
  }
  else if (netwmIsFullScreen() == True) {
    state_ret = GHOST_kWindowStateFullScreen;
  }
  else if (motifIsFullScreen() == True) {
    state_ret = GHOST_kWindowStateFullScreen;
  }
  else if (netwmIsMaximized() == True) {
    state_ret = GHOST_kWindowStateMaximized;
  }
  return state_ret;
}

GHOST_TSuccess GHOST_WindowX11::setState(GHOST_TWindowState state)
{
  GHOST_TWindowState cur_state;
  bool is_max, is_full, is_motif_full;

  cur_state = getState();
  if (state == int(cur_state)) {
    return GHOST_kSuccess;
  }

  if (cur_state != GHOST_kWindowStateMinimized) {
    /*
     * The window don't have this property's
     * if it's not mapped.
     */
    is_max = netwmIsMaximized();
    is_full = netwmIsFullScreen();
  }
  else {
    is_max = False;
    is_full = False;
  }

  is_motif_full = motifIsFullScreen();

  if (state == GHOST_kWindowStateNormal) {
    state = m_normal_state;
  }

  if (state == GHOST_kWindowStateNormal) {
    if (is_max == True) {
      netwmMaximized(False);
    }
    if (is_full == True) {
      netwmFullScreen(False);
    }
    if (is_motif_full == True) {
      motifFullScreen(False);
    }
    icccmSetState(NormalState);
    return GHOST_kSuccess;
  }

  if (state == GHOST_kWindowStateFullScreen) {
    /*
     * We can't change to full screen if the window
     * isn't mapped.
     */
    if (cur_state == GHOST_kWindowStateMinimized) {
      return GHOST_kFailure;
    }

    m_normal_state = cur_state;

    if (is_max == True) {
      netwmMaximized(False);
    }
    if (is_full == False) {
      netwmFullScreen(True);
    }
    if (is_motif_full == False) {
      motifFullScreen(True);
    }
    return GHOST_kSuccess;
  }

  if (state == GHOST_kWindowStateMaximized) {
    /*
     * We can't change to Maximized if the window
     * isn't mapped.
     */
    if (cur_state == GHOST_kWindowStateMinimized) {
      return GHOST_kFailure;
    }

    if (is_full == True) {
      netwmFullScreen(False);
    }
    if (is_motif_full == True) {
      motifFullScreen(False);
    }
    if (is_max == False) {
      netwmMaximized(True);
    }
    return GHOST_kSuccess;
  }

  if (state == GHOST_kWindowStateMinimized) {
    /*
     * The window manager need save the current state of
     * the window (maximized, full screen, etc).
     */
    icccmSetState(IconicState);
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowX11::setOrder(GHOST_TWindowOrder order)
{
  if (order == GHOST_kWindowOrderTop) {
    XWindowAttributes attr;
    Atom atom;

    /* We use both #XRaiseWindow and #_NET_ACTIVE_WINDOW, since some
     * window managers ignore the former (e.g. KWIN from KDE) and others
     * don't implement the latter (e.g. FLUXBOX before 0.9.9). */

    XRaiseWindow(m_display, m_window);

    atom = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", True);

    if (atom != None) {
      Window root;
      XEvent xev;
      long eventmask;

      xev.xclient.type = ClientMessage;
      xev.xclient.serial = 0;
      xev.xclient.send_event = True;
      xev.xclient.window = m_window;
      xev.xclient.message_type = atom;

      xev.xclient.format = 32;
      xev.xclient.data.l[0] = 1;
      xev.xclient.data.l[1] = CurrentTime;
      xev.xclient.data.l[2] = m_window;
      xev.xclient.data.l[3] = 0;
      xev.xclient.data.l[4] = 0;

      root = RootWindow(m_display, m_visualInfo->screen);
      eventmask = SubstructureRedirectMask | SubstructureNotifyMask;

      XSendEvent(m_display, root, False, eventmask, &xev);
    }

    XGetWindowAttributes(m_display, m_window, &attr);

    /* Minimized windows give bad match error. */
    if (attr.map_state == IsViewable) {
      XSetInputFocus(m_display, m_window, RevertToPointerRoot, CurrentTime);
    }
    XFlush(m_display);
  }
  else if (order == GHOST_kWindowOrderBottom) {
    XLowerWindow(m_display, m_window);
    XFlush(m_display);
  }
  else {
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

bool GHOST_WindowX11::isDialog() const
{
  Atom atom_window_type = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
  Atom atom_dialog = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_DIALOG", False);

  Atom *prop_ret;
  ulong bytes_after, num_ret;
  Atom type_ret;
  bool st;
  int format_ret, ret;

  prop_ret = nullptr;
  st = False;
  ret = XGetWindowProperty(m_display,
                           m_window,
                           atom_window_type,
                           0,
                           INT_MAX,
                           False,
                           XA_ATOM,
                           &type_ret,
                           &format_ret,
                           &num_ret,
                           &bytes_after,
                           (uchar **)&prop_ret);
  if ((ret == Success) && (prop_ret) && (format_ret == 32)) {
    if (prop_ret[0] == atom_dialog) {
      st = True;
    }
  }

  if (prop_ret) {
    XFree(prop_ret);
  }

  return st;
}

GHOST_TSuccess GHOST_WindowX11::invalidate()
{
  /* So the idea of this function is to generate an expose event
   * for the window.
   * Unfortunately X does not handle expose events for you and
   * it is the client's job to refresh the dirty part of the window.
   * We need to queue up invalidate calls and generate GHOST events
   * for them in the system.
   *
   * We implement this by setting a boolean in this class to concatenate
   * all such calls into a single event for this window.
   *
   * At the same time we queue the dirty windows in the system class
   * and generate events for them at the next processEvents call. */

  if (m_invalid_window == false) {
    m_system->addDirtyWindow(this);
    m_invalid_window = true;
  }

  return GHOST_kSuccess;
}

/**
 * called by the X11 system implementation when expose events
 * for the window have been pushed onto the GHOST queue
 */

void GHOST_WindowX11::validate()
{
  m_invalid_window = false;
}

GHOST_WindowX11::~GHOST_WindowX11()
{
  std::map<uint, Cursor>::iterator it = m_standard_cursors.begin();
  for (; it != m_standard_cursors.end(); ++it) {
    XFreeCursor(m_display, it->second);
  }

  if (m_empty_cursor) {
    XFreeCursor(m_display, m_empty_cursor);
  }
  if (m_custom_cursor) {
    XFreeCursor(m_display, m_custom_cursor);
  }

  if (m_valid_setup) {
    static Atom Primary_atom, Clipboard_atom;
    Window p_owner, c_owner;
    /* Change the owner of the Atoms to None if we are the owner. */
    Primary_atom = XInternAtom(m_display, "PRIMARY", False);
    Clipboard_atom = XInternAtom(m_display, "CLIPBOARD", False);

    p_owner = XGetSelectionOwner(m_display, Primary_atom);
    c_owner = XGetSelectionOwner(m_display, Clipboard_atom);

    if (p_owner == m_window) {
      XSetSelectionOwner(m_display, Primary_atom, None, CurrentTime);
    }
    if (c_owner == m_window) {
      XSetSelectionOwner(m_display, Clipboard_atom, None, CurrentTime);
    }
  }

  if (m_visualInfo) {
    XFree(m_visualInfo);
  }

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  if (m_xic) {
    XDestroyIC(m_xic);
  }
#endif

#ifdef WITH_XDND
  delete m_dropTarget;
#endif

  releaseNativeHandles();

  if (m_valid_setup) {
    XDestroyWindow(m_display, m_window);
  }
}

GHOST_Context *GHOST_WindowX11::newDrawingContext(GHOST_TDrawingContextType type)
{
  switch (type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(m_wantStereoVisual,
                                                   GHOST_kVulkanPlatformX11,
                                                   m_window,
                                                   m_display,
                                                   NULL,
                                                   NULL,
                                                   1,
                                                   2,
                                                   m_is_debug_context);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_OPENGL_BACKEND
    case GHOST_kDrawingContextTypeOpenGL: {
#  ifdef USE_EGL
      /* Try to initialize an EGL context. */
      for (int minor = 6; minor >= 3; --minor) {
        GHOST_Context *context = GHOST_ContextEGL(
            this->m_system,
            m_wantStereoVisual,
            EGLNativeWindowType(m_window),
            EGLNativeDisplayType(m_display),
            EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            4,
            minor,
            GHOST_OPENGL_EGL_CONTEXT_FLAGS |
                (m_is_debug_context ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0),
            GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
            EGL_OPENGL_API);
        if (context->initializeDrawingContext()) {
          return context;
        }
        delete context;
      }
      /* EGL initialization failed, try to fallback to a GLX context. */
#  endif

      for (int minor = 6; minor >= 3; --minor) {
        GHOST_Context *context = new GHOST_ContextGLX(
            m_wantStereoVisual,
            m_window,
            m_display,
            (GLXFBConfig)m_fbconfig,
            GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            4,
            minor,
            GHOST_OPENGL_GLX_CONTEXT_FLAGS | (m_is_debug_context ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
            GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY);
        if (context->initializeDrawingContext()) {
          return context;
        }
        delete context;
      }
      return nullptr;
    }
#endif

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

GHOST_TSuccess GHOST_WindowX11::getStandardCursor(GHOST_TStandardCursor g_cursor, Cursor &xcursor)
{
  uint xcursor_id;

  switch (g_cursor) {
    case GHOST_kStandardCursorHelp:
      xcursor_id = XC_question_arrow;
      break;
    case GHOST_kStandardCursorWait:
      xcursor_id = XC_watch;
      break;
    case GHOST_kStandardCursorText:
      xcursor_id = XC_xterm;
      break;
    case GHOST_kStandardCursorCrosshair:
      xcursor_id = XC_crosshair;
      break;
    case GHOST_kStandardCursorUpDown:
      xcursor_id = XC_sb_v_double_arrow;
      break;
    case GHOST_kStandardCursorLeftRight:
      xcursor_id = XC_sb_h_double_arrow;
      break;
    case GHOST_kStandardCursorTopSide:
      xcursor_id = XC_top_side;
      break;
    case GHOST_kStandardCursorBottomSide:
      xcursor_id = XC_bottom_side;
      break;
    case GHOST_kStandardCursorLeftSide:
      xcursor_id = XC_left_side;
      break;
    case GHOST_kStandardCursorRightSide:
      xcursor_id = XC_right_side;
      break;
    case GHOST_kStandardCursorTopLeftCorner:
      xcursor_id = XC_top_left_corner;
      break;
    case GHOST_kStandardCursorTopRightCorner:
      xcursor_id = XC_top_right_corner;
      break;
    case GHOST_kStandardCursorBottomRightCorner:
      xcursor_id = XC_bottom_right_corner;
      break;
    case GHOST_kStandardCursorBottomLeftCorner:
      xcursor_id = XC_bottom_left_corner;
      break;
    case GHOST_kStandardCursorDefault:
      xcursor = None;
      return GHOST_kSuccess;
    default:
      xcursor = None;
      return GHOST_kFailure;
  }

  xcursor = m_standard_cursors[xcursor_id];

  if (!xcursor) {
    xcursor = XCreateFontCursor(m_display, xcursor_id);

    m_standard_cursors[xcursor_id] = xcursor;
  }

  return GHOST_kSuccess;
}

Cursor GHOST_WindowX11::getEmptyCursor()
{
  if (!m_empty_cursor) {
    Pixmap blank;
    XColor dummy = {0};
    char data[1] = {0};

    /* make a blank cursor */
    blank = XCreateBitmapFromData(
        m_display, RootWindow(m_display, m_visualInfo->screen), data, 1, 1);

    m_empty_cursor = XCreatePixmapCursor(m_display, blank, blank, &dummy, &dummy, 0, 0);
    XFreePixmap(m_display, blank);
  }

  return m_empty_cursor;
}

GHOST_TSuccess GHOST_WindowX11::setWindowCursorVisibility(bool visible)
{
  Cursor xcursor;

  if (visible) {
    if (m_visible_cursor) {
      xcursor = m_visible_cursor;
    }
    else if (getStandardCursor(getCursorShape(), xcursor) == GHOST_kFailure) {
      getStandardCursor(getCursorShape(), xcursor);
    }
  }
  else {
    xcursor = getEmptyCursor();
  }

  XDefineCursor(m_display, m_window, xcursor);
  XFlush(m_display);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  if (mode != GHOST_kGrabDisable) {
    if (mode != GHOST_kGrabNormal) {
      m_system->getCursorPosition(UNPACK2(m_cursorGrabInitPos));
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide) {
        setWindowCursorVisibility(false);
      }
    }
#ifdef GHOST_X11_GRAB
    XGrabPointer(m_display,
                 m_window,
                 False,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync,
                 GrabModeAsync,
                 None,
                 None,
                 CurrentTime);
#endif
  }
  else {
    if (m_cursorGrab == GHOST_kGrabHide) {
      m_system->setCursorPosition(UNPACK2(m_cursorGrabInitPos));
    }

    if (m_cursorGrab != GHOST_kGrabNormal) {
      /* use to generate a mouse move event, otherwise the last event
       * blender gets can be outside the screen causing menus not to show
       * properly unless the user moves the mouse */

#if defined(WITH_X11_XINPUT) && defined(USE_X11_XINPUT_WARP)
      if ((m_system->m_xinput_version.present) && (m_system->m_xinput_version.major_version >= 2))
      {
        int device_id;
        if (XIGetClientPointer(m_display, None, &device_id) != False) {
          XIWarpPointer(m_display, device_id, None, None, 0, 0, 0, 0, 0, 0);
        }
      }
      else
#endif
      {
        XWarpPointer(m_display, None, None, 0, 0, 0, 0, 0, 0);
      }
    }

    /* Perform this last so to workaround XWayland bug, see: #53004. */
    if (m_cursorGrab == GHOST_kGrabHide) {
      setWindowCursorVisibility(true);
    }

    /* Almost works without but important
     * otherwise the mouse GHOST location can be incorrect on exit. */
    setCursorGrabAccum(0, 0);
    m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1; /* disable */
#ifdef GHOST_X11_GRAB
    XUngrabPointer(m_display, CurrentTime);
#endif
  }

  XFlush(m_display);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  Cursor xcursor;
  if (getStandardCursor(shape, xcursor) == GHOST_kFailure) {
    getStandardCursor(GHOST_kStandardCursorDefault, xcursor);
  }

  m_visible_cursor = xcursor;

  XDefineCursor(m_display, m_window, xcursor);
  XFlush(m_display);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::hasCursorShape(GHOST_TStandardCursor shape)
{
  Cursor xcursor;
  return getStandardCursor(shape, xcursor);
}

GHOST_TSuccess GHOST_WindowX11::setWindowCustomCursorShape(uint8_t *bitmap,
                                                           uint8_t *mask,
                                                           int sizex,
                                                           int sizey,
                                                           int hotX,
                                                           int hotY,
                                                           bool /*canInvertColor*/)
{
  Colormap colormap = DefaultColormap(m_display, m_visualInfo->screen);
  Pixmap bitmap_pix, mask_pix;
  XColor fg, bg;

  if (XAllocNamedColor(m_display, colormap, "White", &fg, &fg) == 0) {
    return GHOST_kFailure;
  }
  if (XAllocNamedColor(m_display, colormap, "Black", &bg, &bg) == 0) {
    return GHOST_kFailure;
  }

  if (m_custom_cursor) {
    XFreeCursor(m_display, m_custom_cursor);
  }

  bitmap_pix = XCreateBitmapFromData(m_display, m_window, (char *)bitmap, sizex, sizey);
  mask_pix = XCreateBitmapFromData(m_display, m_window, (char *)mask, sizex, sizey);

  m_custom_cursor = XCreatePixmapCursor(m_display, bitmap_pix, mask_pix, &fg, &bg, hotX, hotY);
  XDefineCursor(m_display, m_window, m_custom_cursor);
  XFlush(m_display);

  m_visible_cursor = m_custom_cursor;

  XFreePixmap(m_display, bitmap_pix);
  XFreePixmap(m_display, mask_pix);

  XFreeColors(m_display, colormap, &fg.pixel, 1, 0L);
  XFreeColors(m_display, colormap, &bg.pixel, 1, 0L);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::beginFullScreen() const
{
  {
    Window root_return;
    int x_return, y_return;
    uint w_return, h_return, border_w_return, depth_return;

    XGetGeometry(m_display,
                 m_window,
                 &root_return,
                 &x_return,
                 &y_return,
                 &w_return,
                 &h_return,
                 &border_w_return,
                 &depth_return);

    m_system->setCursorPosition(w_return / 2, h_return / 2);
  }

  /* Grab Keyboard & Mouse */
  int err;

  err = XGrabKeyboard(m_display, m_window, False, GrabModeAsync, GrabModeAsync, CurrentTime);
  if (err != GrabSuccess) {
    printf("XGrabKeyboard failed %d\n", err);
  }

  err = XGrabPointer(m_display,
                     m_window,
                     False,
                     PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync,
                     GrabModeAsync,
                     m_window,
                     None,
                     CurrentTime);
  if (err != GrabSuccess) {
    printf("XGrabPointer failed %d\n", err);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::endFullScreen() const
{
  XUngrabKeyboard(m_display, CurrentTime);
  XUngrabPointer(m_display, CurrentTime);

  return GHOST_kSuccess;
}

uint16_t GHOST_WindowX11::getDPIHint()
{
  /* Try to read DPI setting set using xrdb */
  char *resMan = XResourceManagerString(m_display);
  if (resMan) {
    XrmDatabase xrdb = XrmGetStringDatabase(resMan);
    if (xrdb) {
      char *type = nullptr;
      XrmValue val;

      int success = XrmGetResource(xrdb, "Xft.dpi", "Xft.Dpi", &type, &val);
      if (success && type) {
        if (STREQ(type, "String")) {
          return atoi((char *)val.addr);
        }
      }
    }
    XrmDestroyDatabase(xrdb);
  }

  /* Fallback to calculating DPI using X reported DPI, set using `xrandr --dpi`. */
  XWindowAttributes attr;
  if (!XGetWindowAttributes(m_display, m_window, &attr)) {
    /* Failed to get window attributes, return X11 default DPI */
    return 96;
  }

  Screen *screen = attr.screen;
  int pixelWidth = WidthOfScreen(screen);
  int pixelHeight = HeightOfScreen(screen);
  int mmWidth = WidthMMOfScreen(screen);
  int mmHeight = HeightMMOfScreen(screen);

  double pixelDiagonal = sqrt((pixelWidth * pixelWidth) + (pixelHeight * pixelHeight));
  double mmDiagonal = sqrt((mmWidth * mmWidth) + (mmHeight * mmHeight));
  float inchDiagonal = mmDiagonal * 0.039f;
  int dpi = pixelDiagonal / inchDiagonal;
  return dpi;
}

GHOST_TSuccess GHOST_WindowX11::setProgressBar(float progress)
{
  if (m_taskbar.is_valid()) {
    m_taskbar.set_progress(progress);
    m_taskbar.set_progress_enabled(true);
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowX11::endProgressBar()
{
  if (m_taskbar.is_valid()) {
    m_taskbar.set_progress_enabled(false);
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}
