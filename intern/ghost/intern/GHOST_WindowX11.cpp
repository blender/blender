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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

/* For standard X11 cursors */
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef WITH_X11_ALPHA
#  include <X11/extensions/Xrender.h>
#endif
#include "GHOST_WindowX11.h"
#include "GHOST_SystemX11.h"
#include "GHOST_IconX11.h"
#include "STR_String.h"
#include "GHOST_Debug.h"

#ifdef WITH_XDND
#  include "GHOST_DropTargetX11.h"
#endif

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextGLX.h"
#endif

/* for XIWarpPointer */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput2.h>
#endif

//For DPI value
#include <X11/Xresource.h>

#include <cstring>
#include <cstdio>

/* gethostname */
#include <unistd.h>

#include <algorithm>
#include <string>
#include <math.h>

/* For obscure full screen mode stuff
 * lifted verbatim from blut. */

typedef struct {
  long flags;
  long functions;
  long decorations;
  long input_mode;
} MotifWmHints;

#define MWM_HINTS_DECORATIONS (1L << 1)

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

static XVisualInfo *x11_visualinfo_from_glx(Display *display,
                                            bool stereoVisual,
                                            bool needAlpha,
                                            GLXFBConfig *fbconfig)
{
  int glx_major, glx_minor, glx_version; /* GLX version: major.minor */
  int glx_attribs[64];

  *fbconfig = NULL;

  /* Set up the minimum attributes that we require and see if
   * X can find us a visual matching those requirements. */

  if (!glXQueryVersion(display, &glx_major, &glx_minor)) {
    fprintf(stderr,
            "%s:%d: X11 glXQueryVersion() failed, "
            "verify working openGL system!\n",
            __FILE__,
            __LINE__);

    return NULL;
  }
  glx_version = glx_major * 100 + glx_minor;
#ifndef WITH_X11_ALPHA
  (void)glx_version;
#endif

#ifdef WITH_X11_ALPHA
  if (needAlpha && glx_version >= 103 &&
      (glXChooseFBConfig || (glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)glXGetProcAddressARB(
                                 (const GLubyte *)"glXChooseFBConfig")) != NULL) &&
      (glXGetVisualFromFBConfig ||
       (glXGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXGetVisualFromFBConfig")) != NULL)) {

    GHOST_X11_GL_GetAttributes(glx_attribs, 64, stereoVisual, needAlpha, true);

    int nbfbconfig;
    GLXFBConfig *fbconfigs = glXChooseFBConfig(
        display, DefaultScreen(display), glx_attribs, &nbfbconfig);

    /* Any sample level or even zero, which means oversampling disabled, is good
             * but we need a valid visual to continue */
    if (nbfbconfig > 0) {
      /* take a frame buffer config that has alpha cap */
      for (int i = 0; i < nbfbconfig; i++) {
        XVisualInfo *visual = (XVisualInfo *)glXGetVisualFromFBConfig(display, fbconfigs[i]);
        if (!visual)
          continue;
        /* if we don't need a alpha background, the first config will do, otherwise
                     * test the alphaMask as it won't necessarily be present */
        if (needAlpha) {
          XRenderPictFormat *pict_format = XRenderFindVisualFormat(display, visual->visual);
          if (!pict_format)
            continue;
          if (pict_format->direct.alphaMask <= 0)
            continue;
        }

        *fbconfig = fbconfigs[i];
        XFree(fbconfigs);

        return visual;
      }

      XFree(fbconfigs);
    }
  }
  else
#endif
  {
    /* legacy, don't use extension */
    GHOST_X11_GL_GetAttributes(glx_attribs, 64, stereoVisual, needAlpha, false);

    XVisualInfo *visual = glXChooseVisual(display, DefaultScreen(display), glx_attribs);

    /* Any sample level or even zero, which means oversampling disabled, is good
       * but we need a valid visual to continue */
    if (visual != NULL) {
      return visual;
    }
  }

  /* All options exhausted, cannot continue */
  fprintf(stderr,
          "%s:%d: X11 glXChooseVisual() failed, "
          "verify working openGL system!\n",
          __FILE__,
          __LINE__);

  return NULL;
}

GHOST_WindowX11::GHOST_WindowX11(GHOST_SystemX11 *system,
                                 Display *display,
                                 const STR_String &title,
                                 GHOST_TInt32 left,
                                 GHOST_TInt32 top,
                                 GHOST_TUns32 width,
                                 GHOST_TUns32 height,
                                 GHOST_TWindowState state,
                                 const GHOST_TEmbedderWindowID parentWindow,
                                 GHOST_TDrawingContextType type,
                                 const bool stereoVisual,
                                 const bool exclusive,
                                 const bool alphaBackground,
                                 const bool is_debug)
    : GHOST_Window(width, height, state, stereoVisual, exclusive),
      m_display(display),
      m_visualInfo(NULL),
      m_fbconfig(NULL),
      m_normal_state(GHOST_kWindowStateNormal),
      m_system(system),
      m_invalid_window(false),
      m_empty_cursor(None),
      m_custom_cursor(None),
      m_visible_cursor(None),
      m_taskbar("blender.desktop"),
#ifdef WITH_XDND
      m_dropTarget(NULL),
#endif
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      m_xic(NULL),
#endif
      m_valid_setup(false),
      m_is_debug_context(is_debug)
{
  if (type == GHOST_kDrawingContextTypeOpenGL) {
    m_visualInfo = x11_visualinfo_from_glx(
        m_display, stereoVisual, alphaBackground, (GLXFBConfig *)&m_fbconfig);
  }
  else {
    XVisualInfo tmp = {0};
    int n;
    m_visualInfo = XGetVisualInfo(m_display, 0, &tmp, &n);
  }

  /* caller needs to check 'getValid()' */
  if (m_visualInfo == NULL) {
    fprintf(stderr, "initial window could not find the GLX extension\n");
    return;
  }

  unsigned int xattributes_valuemask = 0;

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
  if (parentWindow == 0) {
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
  }
  else {
    Window root_return;
    int x_return, y_return;
    unsigned int w_return, h_return, border_w_return, depth_return;

    XGetGeometry(m_display,
                 parentWindow,
                 &root_return,
                 &x_return,
                 &y_return,
                 &w_return,
                 &h_return,
                 &border_w_return,
                 &depth_return);

    left = 0;
    top = 0;
    width = w_return;
    height = h_return;

    m_window = XCreateWindow(m_display,
                             parentWindow, /* reparent against embedder */
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

    XSelectInput(m_display, parentWindow, SubstructureNotifyMask);
  }

#ifdef WITH_XDND
  /* initialize drop target for newly created window */
  m_dropTarget = new GHOST_DropTargetX11(this, m_system);
  GHOST_PRINT("Set drop target\n");
#endif

  if (state == GHOST_kWindowStateMaximized || state == GHOST_kWindowStateFullScreen) {
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
                    (unsigned char *)atoms,
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
  else if ((state != GHOST_kWindowStateNormal) && (state != GHOST_kWindowStateMinimized)) {
    m_post_init = True;
    m_post_state = state;
  }
  else {
    m_post_init = False;
    m_post_state = GHOST_kWindowStateNormal;
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
    const int len = title.Length() + 1;
    char *wmclass = (char *)malloc(sizeof(char) * len);
    memcpy(wmclass, title.ReadPtr(), len * sizeof(char));
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
      /* printf("Register atoms: %d\n", natom); */
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
                    (unsigned char *)BLENDER_ICONS_WM_X11,
                    sizeof(BLENDER_ICONS_WM_X11) / sizeof(unsigned long));
  }

  /* set the process ID (_NET_WM_PID) */
  {
    Atom _NET_WM_PID = XInternAtom(m_display, "_NET_WM_PID", False);
    pid_t pid = getpid();
    XChangeProperty(m_display,
                    m_window,
                    _NET_WM_PID,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *)&pid,
                    1);
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

  m_tabletData.Active = GHOST_kTabletModeNone;
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
    *(XIC *)ptr = NULL;
  }
  /* Ignored by X11. */
  return True;
}

bool GHOST_WindowX11::createX11_XIC()
{
  XIM xim = m_system->getX11_XIM();
  if (!xim)
    return false;

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
                    NULL);
  if (!m_xic)
    return false;

  unsigned long fevent;
  XGetICValues(m_xic, XNFilterEvents, &fevent, NULL);
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
      /* With modern XInput (xlib 1.6.2 at least and/or evdev 2.9.0) and some 'no-name' tablets
       * like 'UC-LOGIC Tablet WP5540U', we also need to 'select' ButtonPress for motion event,
       * otherwise we do not get any tablet motion event once pen is pressed... See T43367.
       */
      XEventClass ev;

      DeviceMotionNotify(xtablet.Device, xtablet.MotionEvent, ev);
      if (ev)
        xevents.push_back(ev);
      DeviceButtonPress(xtablet.Device, xtablet.PressEvent, ev);
      if (ev)
        xevents.push_back(ev);
      ProximityIn(xtablet.Device, xtablet.ProxInEvent, ev);
      if (ev)
        xevents.push_back(ev);
      ProximityOut(xtablet.Device, xtablet.ProxOutEvent, ev);
      if (ev)
        xevents.push_back(ev);
    }

    XSelectExtensionEvent(m_display, m_window, xevents.data(), (int)xevents.size());
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

void GHOST_WindowX11::setTitle(const STR_String &title)
{
  Atom name = XInternAtom(m_display, "_NET_WM_NAME", 0);
  Atom utf8str = XInternAtom(m_display, "UTF8_STRING", 0);
  XChangeProperty(m_display,
                  m_window,
                  name,
                  utf8str,
                  8,
                  PropModeReplace,
                  (const unsigned char *)title.ReadPtr(),
                  title.Length());

  /* This should convert to valid x11 string
   * and getTitle would need matching change */
  XStoreName(m_display, m_window, title);

  XFlush(m_display);
}

void GHOST_WindowX11::getTitle(STR_String &title) const
{
  char *name = NULL;

  XFetchName(m_display, m_window, &name);
  title = name ? name : "untitled";
  XFree(name);
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
  unsigned int w_return, h_return, border_w_return, depth_return;
  GHOST_TInt32 screen_x, screen_y;

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

GHOST_TSuccess GHOST_WindowX11::setClientWidth(GHOST_TUns32 width)
{
  XWindowChanges values;
  unsigned int value_mask = CWWidth;
  values.width = width;
  XConfigureWindow(m_display, m_window, value_mask, &values);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setClientHeight(GHOST_TUns32 height)
{
  XWindowChanges values;
  unsigned int value_mask = CWHeight;
  values.height = height;
  XConfigureWindow(m_display, m_window, value_mask, &values);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
  XWindowChanges values;
  unsigned int value_mask = CWWidth | CWHeight;
  values.width = width;
  values.height = height;
  XConfigureWindow(m_display, m_window, value_mask, &values);
  return GHOST_kSuccess;
}

void GHOST_WindowX11::screenToClient(GHOST_TInt32 inX,
                                     GHOST_TInt32 inY,
                                     GHOST_TInt32 &outX,
                                     GHOST_TInt32 &outY) const
{
  /* This is correct! */

  int ax, ay;
  Window temp;

  XTranslateCoordinates(
      m_display, RootWindow(m_display, m_visualInfo->screen), m_window, inX, inY, &ax, &ay, &temp);
  outX = ax;
  outY = ay;
}

void GHOST_WindowX11::clientToScreen(GHOST_TInt32 inX,
                                     GHOST_TInt32 inY,
                                     GHOST_TInt32 &outX,
                                     GHOST_TInt32 &outY) const
{
  int ax, ay;
  Window temp;

  XTranslateCoordinates(
      m_display, m_window, RootWindow(m_display, m_visualInfo->screen), inX, inY, &ax, &ay, &temp);
  outX = ax;
  outY = ay;
}

void GHOST_WindowX11::icccmSetState(int state)
{
  XEvent xev;

  if (state != IconicState)
    return;

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

int GHOST_WindowX11::icccmGetState(void) const
{
  struct {
    CARD32 state;
    XID icon;
  } * prop_ret;
  unsigned long bytes_after, num_ret;
  Atom type_ret;
  int ret, format_ret;
  CARD32 st;

  prop_ret = NULL;
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
                           ((unsigned char **)&prop_ret));
  if ((ret == Success) && (prop_ret != NULL) && (num_ret == 2)) {
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

  if (set == True)
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
  else
    xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;

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

bool GHOST_WindowX11::netwmIsMaximized(void) const
{
  Atom *prop_ret;
  unsigned long bytes_after, num_ret, i;
  Atom type_ret;
  bool st;
  int format_ret, ret, count;

  prop_ret = NULL;
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
                           (unsigned char **)&prop_ret);
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

  if (prop_ret)
    XFree(prop_ret);
  return (st);
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

  if (set == True)
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
  else
    xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;

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

bool GHOST_WindowX11::netwmIsFullScreen(void) const
{
  Atom *prop_ret;
  unsigned long bytes_after, num_ret, i;
  Atom type_ret;
  bool st;
  int format_ret, ret;

  prop_ret = NULL;
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
                           (unsigned char **)&prop_ret);
  if ((ret == Success) && (prop_ret) && (format_ret == 32)) {
    for (i = 0; i < num_ret; i++) {
      if (prop_ret[i] == m_system->m_atom._NET_WM_STATE_FULLSCREEN) {
        st = True;
        break;
      }
    }
  }

  if (prop_ret)
    XFree(prop_ret);
  return (st);
}

void GHOST_WindowX11::motifFullScreen(bool set)
{
  MotifWmHints hints;

  hints.flags = MWM_HINTS_DECORATIONS;
  if (set == True)
    hints.decorations = 0;
  else
    hints.decorations = 1;

  XChangeProperty(m_display,
                  m_window,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  m_system->m_atom._MOTIF_WM_HINTS,
                  32,
                  PropModeReplace,
                  (unsigned char *)&hints,
                  4);
}

bool GHOST_WindowX11::motifIsFullScreen(void) const
{
  MotifWmHints *prop_ret;
  unsigned long bytes_after, num_ret;
  Atom type_ret;
  bool state;
  int format_ret, st;

  prop_ret = NULL;
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
                          (unsigned char **)&prop_ret);
  if ((st == Success) && prop_ret) {
    if (prop_ret->flags & MWM_HINTS_DECORATIONS) {
      if (!prop_ret->decorations)
        state = True;
    }
  }

  if (prop_ret)
    XFree(prop_ret);
  return (state);
}

GHOST_TWindowState GHOST_WindowX11::getState() const
{
  GHOST_TWindowState state_ret;
  int state;

  state_ret = GHOST_kWindowStateNormal;
  state = icccmGetState();
  /*
   * In the Iconic and Withdrawn state, the window
   * is unmaped, so only need return a Minimized state.
   */
  if ((state == IconicState) || (state == WithdrawnState))
    state_ret = GHOST_kWindowStateMinimized;
  else if (netwmIsFullScreen() == True)
    state_ret = GHOST_kWindowStateFullScreen;
  else if (motifIsFullScreen() == True)
    state_ret = GHOST_kWindowStateFullScreen;
  else if (netwmIsMaximized() == True)
    state_ret = GHOST_kWindowStateMaximized;
  return (state_ret);
}

GHOST_TSuccess GHOST_WindowX11::setState(GHOST_TWindowState state)
{
  GHOST_TWindowState cur_state;
  bool is_max, is_full, is_motif_full;

  cur_state = getState();
  if (state == (int)cur_state)
    return GHOST_kSuccess;

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

  if (state == GHOST_kWindowStateNormal)
    state = m_normal_state;

  if (state == GHOST_kWindowStateNormal) {
    if (is_max == True)
      netwmMaximized(False);
    if (is_full == True)
      netwmFullScreen(False);
    if (is_motif_full == True)
      motifFullScreen(False);
    icccmSetState(NormalState);
    return (GHOST_kSuccess);
  }

  if (state == GHOST_kWindowStateFullScreen) {
    /*
     * We can't change to full screen if the window
     * isn't mapped.
     */
    if (cur_state == GHOST_kWindowStateMinimized)
      return (GHOST_kFailure);

    m_normal_state = cur_state;

    if (is_max == True)
      netwmMaximized(False);
    if (is_full == False)
      netwmFullScreen(True);
    if (is_motif_full == False)
      motifFullScreen(True);
    return (GHOST_kSuccess);
  }

  if (state == GHOST_kWindowStateMaximized) {
    /*
     * We can't change to Maximized if the window
     * isn't mapped.
     */
    if (cur_state == GHOST_kWindowStateMinimized)
      return (GHOST_kFailure);

    if (is_full == True)
      netwmFullScreen(False);
    if (is_motif_full == True)
      motifFullScreen(False);
    if (is_max == False)
      netwmMaximized(True);
    return (GHOST_kSuccess);
  }

  if (state == GHOST_kWindowStateMinimized) {
    /*
     * The window manager need save the current state of
     * the window (maximized, full screen, etc).
     */
    icccmSetState(IconicState);
    return (GHOST_kSuccess);
  }

  return (GHOST_kFailure);
}

#include <iostream>

GHOST_TSuccess GHOST_WindowX11::setOrder(GHOST_TWindowOrder order)
{
  if (order == GHOST_kWindowOrderTop) {
    XWindowAttributes attr;
    Atom atom;

    /* We use both XRaiseWindow and _NET_ACTIVE_WINDOW, since some
     * window managers ignore the former (e.g. kwin from kde) and others
     * don't implement the latter (e.g. fluxbox pre 0.9.9) */

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

    /* iconized windows give bad match error */
    if (attr.map_state == IsViewable)
      XSetInputFocus(m_display, m_window, RevertToPointerRoot, CurrentTime);
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

/**
 * Destructor.
 * Closes the window and disposes resources allocated.
 */

GHOST_WindowX11::~GHOST_WindowX11()
{
  std::map<unsigned int, Cursor>::iterator it = m_standard_cursors.begin();
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
    /*Change the owner of the Atoms to None if we are the owner*/
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
  if (type == GHOST_kDrawingContextTypeOpenGL) {

    // During development:
    //   try 4.x compatibility profile
    //   try 3.3 compatibility profile
    //   fall back to 3.0 if needed
    //
    // Final Blender 2.8:
    //   try 4.x core profile
    //   try 3.3 core profile
    //   no fallbacks

#if defined(WITH_GL_PROFILE_CORE)
    {
      const char *version_major = (char *)glewGetString(GLEW_VERSION_MAJOR);
      if (version_major != NULL && version_major[0] == '1') {
        fprintf(stderr, "Error: GLEW version 2.0 and above is required.\n");
        abort();
      }
    }
#endif

    const int profile_mask =
#if defined(WITH_GL_PROFILE_CORE)
        GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
#elif defined(WITH_GL_PROFILE_COMPAT)
        GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
#else
#  error  // must specify either core or compat at build time
#endif

    GHOST_Context *context;

    for (int minor = 5; minor >= 0; --minor) {
      context = new GHOST_ContextGLX(m_wantStereoVisual,
                                     m_window,
                                     m_display,
                                     (GLXFBConfig)m_fbconfig,
                                     profile_mask,
                                     4,
                                     minor,
                                     GHOST_OPENGL_GLX_CONTEXT_FLAGS |
                                         (m_is_debug_context ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
                                     GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY);

      if (context->initializeDrawingContext())
        return context;
      else
        delete context;
    }

    context = new GHOST_ContextGLX(m_wantStereoVisual,
                                   m_window,
                                   m_display,
                                   (GLXFBConfig)m_fbconfig,
                                   profile_mask,
                                   3,
                                   3,
                                   GHOST_OPENGL_GLX_CONTEXT_FLAGS |
                                       (m_is_debug_context ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
                                   GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY);

    if (context->initializeDrawingContext())
      return context;
    else
      delete context;

    /* Ugly, but we get crashes unless a whole bunch of systems are patched. */
    fprintf(stderr, "Error! Unsupported graphics card or driver.\n");
    fprintf(stderr,
            "A graphics card and driver with support for OpenGL 3.3 or higher is required.\n");
    fprintf(stderr, "The program will now close.\n");
    fflush(stderr);
    exit(1);
  }

  return NULL;
}

Cursor GHOST_WindowX11::getStandardCursor(GHOST_TStandardCursor g_cursor)
{
  unsigned int xcursor_id;

#define GtoX(gcurs, xcurs) \
  case gcurs: \
    xcursor_id = xcurs
  switch (g_cursor) {
    GtoX(GHOST_kStandardCursorRightArrow, XC_arrow);
    break;
    GtoX(GHOST_kStandardCursorLeftArrow, XC_top_left_arrow);
    break;
    GtoX(GHOST_kStandardCursorInfo, XC_hand1);
    break;
    GtoX(GHOST_kStandardCursorDestroy, XC_pirate);
    break;
    GtoX(GHOST_kStandardCursorHelp, XC_question_arrow);
    break;
    GtoX(GHOST_kStandardCursorCycle, XC_exchange);
    break;
    GtoX(GHOST_kStandardCursorSpray, XC_spraycan);
    break;
    GtoX(GHOST_kStandardCursorWait, XC_watch);
    break;
    GtoX(GHOST_kStandardCursorText, XC_xterm);
    break;
    GtoX(GHOST_kStandardCursorCrosshair, XC_crosshair);
    break;
    GtoX(GHOST_kStandardCursorUpDown, XC_sb_v_double_arrow);
    break;
    GtoX(GHOST_kStandardCursorLeftRight, XC_sb_h_double_arrow);
    break;
    GtoX(GHOST_kStandardCursorTopSide, XC_top_side);
    break;
    GtoX(GHOST_kStandardCursorBottomSide, XC_bottom_side);
    break;
    GtoX(GHOST_kStandardCursorLeftSide, XC_left_side);
    break;
    GtoX(GHOST_kStandardCursorRightSide, XC_right_side);
    break;
    GtoX(GHOST_kStandardCursorTopLeftCorner, XC_top_left_corner);
    break;
    GtoX(GHOST_kStandardCursorTopRightCorner, XC_top_right_corner);
    break;
    GtoX(GHOST_kStandardCursorBottomRightCorner, XC_bottom_right_corner);
    break;
    GtoX(GHOST_kStandardCursorBottomLeftCorner, XC_bottom_left_corner);
    break;
    GtoX(GHOST_kStandardCursorPencil, XC_pencil);
    break;
    GtoX(GHOST_kStandardCursorCopy, XC_arrow);
    break;
    default:
      xcursor_id = 0;
  }
#undef GtoX

  if (xcursor_id) {
    Cursor xcursor = m_standard_cursors[xcursor_id];

    if (!xcursor) {
      xcursor = XCreateFontCursor(m_display, xcursor_id);

      m_standard_cursors[xcursor_id] = xcursor;
    }

    return xcursor;
  }
  else {
    return None;
  }
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
    if (m_visible_cursor)
      xcursor = m_visible_cursor;
    else
      xcursor = getStandardCursor(getCursorShape());
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
      m_system->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide)
        setWindowCursorVisibility(false);
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
      m_system->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
    }

    if (m_cursorGrab != GHOST_kGrabNormal) {
      /* use to generate a mouse move event, otherwise the last event
       * blender gets can be outside the screen causing menus not to show
       * properly unless the user moves the mouse */

#if defined(WITH_X11_XINPUT) && defined(USE_X11_XINPUT_WARP)
      if ((m_system->m_xinput_version.present) &&
          (m_system->m_xinput_version.major_version >= 2)) {
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

    /* Perform this last so to workaround XWayland bug, see: T53004. */
    if (m_cursorGrab == GHOST_kGrabHide) {
      setWindowCursorVisibility(true);
    }

    /* Almost works without but important otherwise the mouse GHOST location can be incorrect on exit */
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
  Cursor xcursor = getStandardCursor(shape);

  m_visible_cursor = xcursor;

  XDefineCursor(m_display, m_window, xcursor);
  XFlush(m_display);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
                                                           GHOST_TUns8 mask[16][2],
                                                           int hotX,
                                                           int hotY)
{
  setWindowCustomCursorShape((GHOST_TUns8 *)bitmap, (GHOST_TUns8 *)mask, 16, 16, hotX, hotY, 0, 1);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                                           GHOST_TUns8 *mask,
                                                           int sizex,
                                                           int sizey,
                                                           int hotX,
                                                           int hotY,
                                                           int /*fg_color*/,
                                                           int /*bg_color*/)
{
  Colormap colormap = DefaultColormap(m_display, m_visualInfo->screen);
  Pixmap bitmap_pix, mask_pix;
  XColor fg, bg;

  if (XAllocNamedColor(m_display, colormap, "White", &fg, &fg) == 0)
    return GHOST_kFailure;
  if (XAllocNamedColor(m_display, colormap, "Black", &bg, &bg) == 0)
    return GHOST_kFailure;

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
    unsigned int w_return, h_return, border_w_return, depth_return;

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
  if (err != GrabSuccess)
    printf("XGrabKeyboard failed %d\n", err);

  err = XGrabPointer(m_display,
                     m_window,
                     False,
                     PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                     GrabModeAsync,
                     GrabModeAsync,
                     m_window,
                     None,
                     CurrentTime);
  if (err != GrabSuccess)
    printf("XGrabPointer failed %d\n", err);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowX11::endFullScreen() const
{
  XUngrabKeyboard(m_display, CurrentTime);
  XUngrabPointer(m_display, CurrentTime);

  return GHOST_kSuccess;
}

GHOST_TUns16 GHOST_WindowX11::getDPIHint()
{
  /* Try to read DPI setting set using xrdb */
  char *resMan = XResourceManagerString(m_display);
  if (resMan) {
    XrmDatabase xrdb = XrmGetStringDatabase(resMan);
    if (xrdb) {
      char *type = NULL;
      XrmValue val;

      int success = XrmGetResource(xrdb, "Xft.dpi", "Xft.Dpi", &type, &val);
      if (success && type) {
        if (strcmp(type, "String") == 0) {
          return atoi((char *)val.addr);
        }
      }
    }
    XrmDestroyDatabase(xrdb);
  }

  /* Fallback to calculating DPI using X reported DPI, set using xrandr --dpi */
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
