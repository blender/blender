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
 * Part of this code has been taken from Qt, under LGPL license
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 */

/** \file
 * \ingroup GHOST
 */

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h> /* allow detectable autorepeate */
#include <X11/Xutil.h>

#include "GHOST_SystemX11.h"
#include "GHOST_WindowX11.h"
#include "GHOST_WindowManager.h"
#include "GHOST_TimerManager.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventWheel.h"
#include "GHOST_DisplayManagerX11.h"
#include "GHOST_EventDragnDrop.h"
#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerUnix.h"
#endif

#ifdef WITH_XDND
#  include "GHOST_DropTargetX11.h"
#endif

#include "GHOST_Debug.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextGLX.h"
#endif

#ifdef WITH_XF86KEYSYM
#  include <X11/XF86keysym.h>
#endif

#ifdef WITH_X11_XFIXES
#  include <X11/extensions/Xfixes.h>
/* Workaround for XWayland grab glitch: T53004. */
#  define WITH_XWAYLAND_HACK
#endif

/* for XIWarpPointer */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput2.h>
#endif

/* For timing */
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <stdio.h> /* for fprintf only */
#include <cstdlib> /* for exit */

/* for debugging - so we can breakpoint X11 errors */
// #define USE_X11_ERROR_HANDLERS

#ifdef WITH_X11_XINPUT
#  define USE_XINPUT_HOTPLUG
#endif

/* see [#34039] Fix Alt key glitch on Unity desktop */
#define USE_UNITY_WORKAROUND

/* Fix 'shortcut' part of keyboard reading code only ever using first defined keymap
 * instead of active one. See T47228 and D1746 */
#define USE_NON_LATIN_KB_WORKAROUND

static GHOST_TKey ghost_key_from_keysym(const KeySym key);
static GHOST_TKey ghost_key_from_keycode(const XkbDescPtr xkb_descr, const KeyCode keycode);
static GHOST_TKey ghost_key_from_keysym_or_keycode(const KeySym key,
                                                   const XkbDescPtr xkb_descr,
                                                   const KeyCode keycode);

/* these are for copy and select copy */
static char *txt_cut_buffer = NULL;
static char *txt_select_buffer = NULL;

#ifdef WITH_XWAYLAND_HACK
static bool use_xwayland_hack = false;
#endif

using namespace std;

GHOST_SystemX11::GHOST_SystemX11() : GHOST_System(), m_xkb_descr(NULL), m_start_time(0)
{
  XInitThreads();
  m_display = XOpenDisplay(NULL);

  if (!m_display) {
    std::cerr << "Unable to open a display" << std::endl;
    abort(); /* was return before, but this would just mean it will crash later */
  }

#ifdef USE_X11_ERROR_HANDLERS
  (void)XSetErrorHandler(GHOST_X11_ApplicationErrorHandler);
  (void)XSetIOErrorHandler(GHOST_X11_ApplicationIOErrorHandler);
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  /* note -- don't open connection to XIM server here, because the locale
   * has to be set before opening the connection but setlocale() has not
   * been called yet.  the connection will be opened after entering
   * the event loop. */
  m_xim = NULL;
#endif

#define GHOST_INTERN_ATOM_IF_EXISTS(atom) \
  { \
    m_atom.atom = XInternAtom(m_display, #atom, True); \
  } \
  (void)0
#define GHOST_INTERN_ATOM(atom) \
  { \
    m_atom.atom = XInternAtom(m_display, #atom, False); \
  } \
  (void)0

  GHOST_INTERN_ATOM_IF_EXISTS(WM_DELETE_WINDOW);
  GHOST_INTERN_ATOM(WM_PROTOCOLS);
  GHOST_INTERN_ATOM(WM_TAKE_FOCUS);
  GHOST_INTERN_ATOM(WM_STATE);
  GHOST_INTERN_ATOM(WM_CHANGE_STATE);
  GHOST_INTERN_ATOM(_NET_WM_STATE);
  GHOST_INTERN_ATOM(_NET_WM_STATE_MAXIMIZED_HORZ);
  GHOST_INTERN_ATOM(_NET_WM_STATE_MAXIMIZED_VERT);

  GHOST_INTERN_ATOM(_NET_WM_STATE_FULLSCREEN);
  GHOST_INTERN_ATOM(_MOTIF_WM_HINTS);
  GHOST_INTERN_ATOM(TARGETS);
  GHOST_INTERN_ATOM(STRING);
  GHOST_INTERN_ATOM(COMPOUND_TEXT);
  GHOST_INTERN_ATOM(TEXT);
  GHOST_INTERN_ATOM(CLIPBOARD);
  GHOST_INTERN_ATOM(PRIMARY);
  GHOST_INTERN_ATOM(XCLIP_OUT);
  GHOST_INTERN_ATOM(INCR);
  GHOST_INTERN_ATOM(UTF8_STRING);
#ifdef WITH_X11_XINPUT
  m_atom.TABLET = XInternAtom(m_display, XI_TABLET, False);
#endif

#undef GHOST_INTERN_ATOM_IF_EXISTS
#undef GHOST_INTERN_ATOM

  m_last_warp = 0;
  m_last_release_keycode = 0;
  m_last_release_time = 0;

  /* compute the initial time */
  timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    GHOST_ASSERT(false, "Could not instantiate timer!");
  }

  /* Taking care not to overflow the tv.tv_sec * 1000 */
  m_start_time = GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000;

  /* use detectable autorepeate, mac and windows also do this */
  int use_xkb;
  int xkb_opcode, xkb_event, xkb_error;
  int xkb_major = XkbMajorVersion, xkb_minor = XkbMinorVersion;

  use_xkb = XkbQueryExtension(
      m_display, &xkb_opcode, &xkb_event, &xkb_error, &xkb_major, &xkb_minor);
  if (use_xkb) {
    XkbSetDetectableAutoRepeat(m_display, true, NULL);

    m_xkb_descr = XkbGetMap(m_display, 0, XkbUseCoreKbd);
    if (m_xkb_descr) {
      XkbGetNames(m_display, XkbKeyNamesMask, m_xkb_descr);
    }
  }

#ifdef WITH_XWAYLAND_HACK
  use_xwayland_hack = getenv("WAYLAND_DISPLAY") != NULL;
#endif

#ifdef WITH_X11_XINPUT
  /* detect if we have xinput (for reuse) */
  {
    memset(&m_xinput_version, 0, sizeof(m_xinput_version));
    XExtensionVersion *version = XGetExtensionVersion(m_display, INAME);
    if (version && (version != (XExtensionVersion *)NoSuchExtension)) {
      if (version->present) {
        m_xinput_version = *version;
      }
      XFree(version);
    }
  }

#  ifdef USE_XINPUT_HOTPLUG
  if (m_xinput_version.present) {
    XEventClass class_presence;
    int xi_presence;
    DevicePresence(m_display, xi_presence, class_presence);
    XSelectExtensionEvent(
        m_display, RootWindow(m_display, DefaultScreen(m_display)), &class_presence, 1);
    (void)xi_presence;
  }
#  endif /* USE_XINPUT_HOTPLUG */

  refreshXInputDevices();
#endif /* WITH_X11_XINPUT */
}

GHOST_SystemX11::~GHOST_SystemX11()
{
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  if (m_xim) {
    XCloseIM(m_xim);
  }
#endif

#ifdef WITH_X11_XINPUT
  /* Close tablet devices. */
  clearXInputDevices();
#endif /* WITH_X11_XINPUT */

  if (m_xkb_descr) {
    XkbFreeKeyboard(m_xkb_descr, XkbAllComponentsMask, true);
  }

  XCloseDisplay(m_display);
}

GHOST_TSuccess GHOST_SystemX11::init()
{
  GHOST_TSuccess success = GHOST_System::init();

  if (success) {
#ifdef WITH_INPUT_NDOF
    m_ndofManager = new GHOST_NDOFManagerUnix(*this);
#endif
    m_displayManager = new GHOST_DisplayManagerX11(this);

    if (m_displayManager) {
      return GHOST_kSuccess;
    }
  }

  return GHOST_kFailure;
}

GHOST_TUns64 GHOST_SystemX11::getMilliSeconds() const
{
  timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    GHOST_ASSERT(false, "Could not compute time!");
  }

  /* Taking care not to overflow the tv.tv_sec * 1000 */
  return GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000 - m_start_time;
}

GHOST_TUns8 GHOST_SystemX11::getNumDisplays() const
{
  return GHOST_TUns8(1);
}

/**
 * Returns the dimensions of the main display on this system.
 * \return The dimension of the main display.
 */
void GHOST_SystemX11::getMainDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  if (m_display) {
    /* note, for this to work as documented,
     * we would need to use Xinerama check r54370 for code that did this,
     * we've since removed since its not worth the extra dep - campbell */
    getAllDisplayDimensions(width, height);
  }
}

/**
 * Returns the dimensions of the main display on this system.
 * \return The dimension of the main display.
 */
void GHOST_SystemX11::getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  if (m_display) {
    width = DisplayWidth(m_display, DefaultScreen(m_display));
    height = DisplayHeight(m_display, DefaultScreen(m_display));
  }
}

/**
 * Create a new window.
 * The new window is added to the list of windows managed.
 * Never explicitly delete the window, use disposeWindow() instead.
 * \param   title   The name of the window
 * (displayed in the title bar of the window if the OS supports it).
 * \param   left    The coordinate of the left edge of the window.
 * \param   top     The coordinate of the top edge of the window.
 * \param   width   The width the window.
 * \param   height  The height the window.
 * \param   state   The state of the window when opened.
 * \param   type    The type of drawing context installed in this window.
 * \param glSettings: Misc OpenGL settings.
 * \param exclusive: Use to show the window ontop and ignore others (used fullscreen).
 * \param   parentWindow    Parent (embedder) window
 * \return  The new window (or 0 if creation failed).
 */
GHOST_IWindow *GHOST_SystemX11::createWindow(const STR_String &title,
                                             GHOST_TInt32 left,
                                             GHOST_TInt32 top,
                                             GHOST_TUns32 width,
                                             GHOST_TUns32 height,
                                             GHOST_TWindowState state,
                                             GHOST_TDrawingContextType type,
                                             GHOST_GLSettings glSettings,
                                             const bool exclusive,
                                             const GHOST_TEmbedderWindowID parentWindow)
{
  GHOST_WindowX11 *window = NULL;

  if (!m_display)
    return 0;

  window = new GHOST_WindowX11(this,
                               m_display,
                               title,
                               left,
                               top,
                               width,
                               height,
                               state,
                               parentWindow,
                               type,
                               ((glSettings.flags & GHOST_glStereoVisual) != 0),
                               exclusive,
                               ((glSettings.flags & GHOST_glAlphaBackground) != 0),
                               (glSettings.flags & GHOST_glDebugContext) != 0);

  if (window) {
    /* Both are now handle in GHOST_WindowX11.cpp
     * Focus and Delete atoms. */

    if (window->getValid()) {
      /* Store the pointer to the window */
      m_windowManager->addWindow(window);
      m_windowManager->setActiveWindow(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      delete window;
      window = NULL;
    }
  }
  return window;
}

bool GHOST_SystemX11::supportsNativeDialogs(void)
{
  return false;
}

/**
 * Create a new offscreen context.
 * Never explicitly delete the context, use disposeContext() instead.
 * \return  The new context (or 0 if creation failed).
 */
GHOST_IContext *GHOST_SystemX11::createOffscreenContext()
{
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
    context = new GHOST_ContextGLX(false,
                                   (Window)NULL,
                                   m_display,
                                   (GLXFBConfig)NULL,
                                   profile_mask,
                                   4,
                                   minor,
                                   GHOST_OPENGL_GLX_CONTEXT_FLAGS |
                                       (false ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
                                   GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY);

    if (context->initializeDrawingContext())
      return context;
    else
      delete context;
  }

  context = new GHOST_ContextGLX(false,
                                 (Window)NULL,
                                 m_display,
                                 (GLXFBConfig)NULL,
                                 profile_mask,
                                 3,
                                 3,
                                 GHOST_OPENGL_GLX_CONTEXT_FLAGS |
                                     (false ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
                                 GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY);

  if (context->initializeDrawingContext())
    return context;
  else
    delete context;

  return NULL;
}

/**
 * Dispose of a context.
 * \param   context Pointer to the context to be disposed.
 * \return  Indication of success.
 */
GHOST_TSuccess GHOST_SystemX11::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
static void destroyIMCallback(XIM /*xim*/, XPointer ptr, XPointer /*data*/)
{
  GHOST_PRINT("XIM server died\n");

  if (ptr)
    *(XIM *)ptr = NULL;
}

bool GHOST_SystemX11::openX11_IM()
{
  if (!m_display)
    return false;

  /* set locale modifiers such as "@im=ibus" specified by XMODIFIERS */
  XSetLocaleModifiers("");

  m_xim = XOpenIM(m_display, NULL, (char *)GHOST_X11_RES_NAME, (char *)GHOST_X11_RES_CLASS);
  if (!m_xim)
    return false;

  XIMCallback destroy;
  destroy.callback = (XIMProc)destroyIMCallback;
  destroy.client_data = (XPointer)&m_xim;
  XSetIMValues(m_xim, XNDestroyCallback, &destroy, NULL);
  return true;
}
#endif

GHOST_WindowX11 *GHOST_SystemX11::findGhostWindow(Window xwind) const
{

  if (xwind == 0)
    return NULL;

  /* It is not entirely safe to do this as the backptr may point
   * to a window that has recently been removed.
   * We should always check the window manager's list of windows
   * and only process events on these windows. */

  vector<GHOST_IWindow *> &win_vec = m_windowManager->getWindows();

  vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
  vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();

  for (; win_it != win_end; ++win_it) {
    GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
    if (window->getXWindow() == xwind) {
      return window;
    }
  }
  return NULL;
}

static void SleepTillEvent(Display *display, GHOST_TInt64 maxSleep)
{
  int fd = ConnectionNumber(display);
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  if (maxSleep == -1) {
    select(fd + 1, &fds, NULL, NULL, NULL);
  }
  else {
    timeval tv;

    tv.tv_sec = maxSleep / 1000;
    tv.tv_usec = (maxSleep - tv.tv_sec * 1000) * 1000;

    select(fd + 1, &fds, NULL, NULL, &tv);
  }
}

/* This function borrowed from Qt's X11 support
 * qclipboard_x11.cpp
 *  */
struct init_timestamp_data {
  Time timestamp;
};

static Bool init_timestamp_scanner(Display *, XEvent *event, XPointer arg)
{
  init_timestamp_data *data = reinterpret_cast<init_timestamp_data *>(arg);
  switch (event->type) {
    case ButtonPress:
    case ButtonRelease:
      data->timestamp = event->xbutton.time;
      break;
    case MotionNotify:
      data->timestamp = event->xmotion.time;
      break;
    case KeyPress:
    case KeyRelease:
      data->timestamp = event->xkey.time;
      break;
    case PropertyNotify:
      data->timestamp = event->xproperty.time;
      break;
    case EnterNotify:
    case LeaveNotify:
      data->timestamp = event->xcrossing.time;
      break;
    case SelectionClear:
      data->timestamp = event->xselectionclear.time;
      break;
    default:
      break;
  }

  return false;
}

Time GHOST_SystemX11::lastEventTime(Time default_time)
{
  init_timestamp_data data;
  data.timestamp = default_time;
  XEvent ev;
  XCheckIfEvent(m_display, &ev, &init_timestamp_scanner, (XPointer)&data);

  return data.timestamp;
}

bool GHOST_SystemX11::processEvents(bool waitForEvent)
{
  /* Get all the current events -- translate them into
   * ghost events and call base class pushEvent() method. */

  bool anyProcessed = false;

  do {
    GHOST_TimerManager *timerMgr = getTimerManager();

    if (waitForEvent && m_dirty_windows.empty() && !XPending(m_display)) {
      GHOST_TUns64 next = timerMgr->nextFireTime();

      if (next == GHOST_kFireTimeNever) {
        SleepTillEvent(m_display, -1);
      }
      else {
        GHOST_TInt64 maxSleep = next - getMilliSeconds();

        if (maxSleep >= 0)
          SleepTillEvent(m_display, next - getMilliSeconds());
      }
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      anyProcessed = true;
    }

    while (XPending(m_display)) {
      XEvent xevent;
      XNextEvent(m_display, &xevent);

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* open connection to XIM server and create input context (XIC)
       * when receiving the first FocusIn or KeyPress event after startup,
       * or recover XIM and XIC when the XIM server has been restarted */
      if (xevent.type == FocusIn || xevent.type == KeyPress) {
        if (!m_xim && openX11_IM()) {
          GHOST_PRINT("Connected to XIM server\n");
        }

        if (m_xim) {
          GHOST_WindowX11 *window = findGhostWindow(xevent.xany.window);
          if (window && !window->getX11_XIC() && window->createX11_XIC()) {
            GHOST_PRINT("XIM input context created\n");
            if (xevent.type == KeyPress)
              /* we can assume the window has input focus
               * here, because key events are received only
               * when the window is focused. */
              XSetICFocus(window->getX11_XIC());
          }
        }
      }

      /* dispatch event to XIM server */
      if ((XFilterEvent(&xevent, (Window)NULL) == True)) {
        /* do nothing now, the event is consumed by XIM. */
        continue;
      }
#endif
      /* when using autorepeat, some keypress events can actually come *after* the
       * last keyrelease. The next code takes care of that */
      if (xevent.type == KeyRelease) {
        m_last_release_keycode = xevent.xkey.keycode;
        m_last_release_time = xevent.xkey.time;
      }
      else if (xevent.type == KeyPress) {
        if ((xevent.xkey.keycode == m_last_release_keycode) &&
            ((xevent.xkey.time <= m_last_release_time)))
          continue;
      }

      processEvent(&xevent);
      anyProcessed = true;

#ifdef USE_UNITY_WORKAROUND
      /* note: processEvent() can't include this code because
       * KeymapNotify event have no valid window information. */

      /* the X server generates KeymapNotify event immediately after
       * every EnterNotify and FocusIn event.  we handle this event
       * to correct modifier states. */
      if (xevent.type == FocusIn) {
        /* use previous event's window, because KeymapNotify event
         * has no window information. */
        GHOST_WindowX11 *window = findGhostWindow(xevent.xany.window);
        if (window && XPending(m_display) >= 2) {
          XNextEvent(m_display, &xevent);

          if (xevent.type == KeymapNotify) {
            XEvent xev_next;

            /* check if KeyPress or KeyRelease event was generated
             * in order to confirm the window is active. */
            XPeekEvent(m_display, &xev_next);

            if (xev_next.type == KeyPress || xev_next.type == KeyRelease) {
              /* XK_Hyper_L/R currently unused */
              const static KeySym modifiers[8] = {
                  XK_Shift_L,
                  XK_Shift_R,
                  XK_Control_L,
                  XK_Control_R,
                  XK_Alt_L,
                  XK_Alt_R,
                  XK_Super_L,
                  XK_Super_R,
              };

              for (int i = 0; i < (sizeof(modifiers) / sizeof(*modifiers)); i++) {
                KeyCode kc = XKeysymToKeycode(m_display, modifiers[i]);
                if (((xevent.xkeymap.key_vector[kc >> 3] >> (kc & 7)) & 1) != 0) {
                  pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                               GHOST_kEventKeyDown,
                                               window,
                                               ghost_key_from_keysym(modifiers[i]),
                                               '\0',
                                               NULL));
                }
              }
            }
          }
        }
      }
#endif /* USE_UNITY_WORKAROUND */
    }

    if (generateWindowExposeEvents()) {
      anyProcessed = true;
    }

#ifdef WITH_INPUT_NDOF
    if (static_cast<GHOST_NDOFManagerUnix *>(m_ndofManager)->processEvents()) {
      anyProcessed = true;
    }
#endif

  } while (waitForEvent && !anyProcessed);

  return anyProcessed;
}

#ifdef WITH_X11_XINPUT
static bool checkTabletProximity(Display *display, XDevice *device)
{
  /* we could have true/false/not-found return value, but for now false is OK */

  /* see: state.c from xinput, to get more data out of the device */
  XDeviceState *state;

  if (device == NULL) {
    return false;
  }

  /* needed since unplugging will abort() without this */
  GHOST_X11_ERROR_HANDLERS_OVERRIDE(handler_store);

  state = XQueryDeviceState(display, device);

  GHOST_X11_ERROR_HANDLERS_RESTORE(handler_store);

  if (state) {
    XInputClass *cls = state->data;
    // printf("%d class%s :\n", state->num_classes,
    //       (state->num_classes > 1) ? "es" : "");
    for (int loop = 0; loop < state->num_classes; loop++) {
      switch (cls->c_class) {
        case ValuatorClass:
          XValuatorState *val_state = (XValuatorState *)cls;
          // printf("ValuatorClass Mode=%s Proximity=%s\n",
          //        val_state->mode & 1 ? "Absolute" : "Relative",
          //        val_state->mode & 2 ? "Out" : "In");

          if ((val_state->mode & 2) == 0) {
            XFreeDeviceState(state);
            return true;
          }
          break;
      }
      cls = (XInputClass *)((char *)cls + cls->length);
    }
    XFreeDeviceState(state);
  }
  return false;
}
#endif /* WITH_X11_XINPUT */

void GHOST_SystemX11::processEvent(XEvent *xe)
{
  GHOST_WindowX11 *window = findGhostWindow(xe->xany.window);
  GHOST_Event *g_event = NULL;

#ifdef USE_XINPUT_HOTPLUG
  /* Hot-Plug support */
  if (m_xinput_version.present) {
    XEventClass class_presence;
    int xi_presence;

    DevicePresence(m_display, xi_presence, class_presence);
    (void)class_presence;

    if (xe->type == xi_presence) {
      XDevicePresenceNotifyEvent *notify_event = (XDevicePresenceNotifyEvent *)xe;
      if ((notify_event->devchange == DeviceEnabled) ||
          (notify_event->devchange == DeviceDisabled) ||
          (notify_event->devchange == DeviceAdded) || (notify_event->devchange == DeviceRemoved)) {
        refreshXInputDevices();

        /* update all window events */
        {
          vector<GHOST_IWindow *> &win_vec = m_windowManager->getWindows();
          vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
          vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();

          for (; win_it != win_end; ++win_it) {
            GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
            window->refreshXInputDevices();
          }
        }
      }
    }
  }
#endif /* USE_XINPUT_HOTPLUG */

  if (!window) {
    return;
  }

#ifdef WITH_X11_XINPUT
  /* Proximity-Out Events are not reliable, if the tablet is active - check on each event
   * this adds a little overhead but only while the tablet is in use.
   * in the future we could have a ghost call window->CheckTabletProximity()
   * but for now enough parts of the code are checking 'Active'
   * - campbell */
  if (window->GetTabletData()->Active != GHOST_kTabletModeNone) {
    bool any_proximity = false;

    for (GHOST_TabletX11 &xtablet : m_xtablets) {
      if (checkTabletProximity(xe->xany.display, xtablet.Device)) {
        any_proximity = true;
      }
    }

    if (!any_proximity) {
      // printf("proximity disable\n");
      window->GetTabletData()->Active = GHOST_kTabletModeNone;
    }
  }
#endif /* WITH_X11_XINPUT */
  switch (xe->type) {
    case Expose: {
      XExposeEvent &xee = xe->xexpose;

      if (xee.count == 0) {
        /* Only generate a single expose event
         * per read of the event queue. */

        g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window);
      }
      break;
    }

    case MotionNotify: {
      XMotionEvent &xme = xe->xmotion;

#ifdef WITH_X11_XINPUT
      bool is_tablet = window->GetTabletData()->Active != GHOST_kTabletModeNone;
#else
      bool is_tablet = false;
#endif

      if (is_tablet == false && window->getCursorGrabModeIsWarp()) {
        GHOST_TInt32 x_new = xme.x_root;
        GHOST_TInt32 y_new = xme.y_root;
        GHOST_TInt32 x_accum, y_accum;
        GHOST_Rect bounds;

        /* fallback to window bounds */
        if (window->getCursorGrabBounds(bounds) == GHOST_kFailure)
          window->getClientBounds(bounds);

        /* could also clamp to screen bounds
         * wrap with a window outside the view will fail atm  */
        bounds.wrapPoint(x_new, y_new, 8); /* offset of one incase blender is at screen bounds */
        window->getCursorGrabAccum(x_accum, y_accum);

        if (x_new != xme.x_root || y_new != xme.y_root) {
          if (xme.time > m_last_warp) {
            /* when wrapping we don't need to add an event because the
             * setCursorPosition call will cause a new event after */
            setCursorPosition(x_new, y_new); /* wrap */
            window->setCursorGrabAccum(x_accum + (xme.x_root - x_new),
                                       y_accum + (xme.y_root - y_new));
            m_last_warp = lastEventTime(xme.time);
          }
          else {
            setCursorPosition(x_new, y_new); /* wrap but don't accumulate */
          }
        }
        else {
          g_event = new GHOST_EventCursor(getMilliSeconds(),
                                          GHOST_kEventCursorMove,
                                          window,
                                          xme.x_root + x_accum,
                                          xme.y_root + y_accum);
        }
      }
      else {
        g_event = new GHOST_EventCursor(
            getMilliSeconds(), GHOST_kEventCursorMove, window, xme.x_root, xme.y_root);
      }
      break;
    }

    case KeyPress:
    case KeyRelease: {
      XKeyEvent *xke = &(xe->xkey);
      KeySym key_sym;
      char ascii;
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* utf8_array[] is initial buffer used for Xutf8LookupString().
       * if the length of the utf8 string exceeds this array, allocate
       * another memory area and call Xutf8LookupString() again.
       * the last 5 bytes are used to avoid segfault that might happen
       * at the end of this buffer when the constructor of GHOST_EventKey
       * reads 6 bytes regardless of the effective data length. */
      char utf8_array[16 * 6 + 5]; /* 16 utf8 characters */
      char *utf8_buf = utf8_array;
      int len = 1; /* at least one null character will be stored */
#else
      char *utf8_buf = NULL;
#endif

      GHOST_TEventType type = (xke->type == KeyPress) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;

      GHOST_TKey gkey;

#ifdef USE_NON_LATIN_KB_WORKAROUND
      /* XXX Code below is kinda awfully convoluted... Issues are:
       *
       *     - In keyboards like latin ones, numbers need a 'Shift' to be accessed but key_sym
       *       is unmodified (or anyone swapping the keys with xmodmap).
       *
       *     - XLookupKeysym seems to always use first defined keymap (see T47228), which generates
       *       keycodes unusable by ghost_key_from_keysym for non-latin-compatible keymaps.
       *
       * To address this, we:
       *
       *     - Try to get a 'number' key_sym using XLookupKeysym (with virtual shift modifier),
       *       in a very restrictive set of cases.
       *     - Fallback to XLookupString to get a key_sym from active user-defined keymap.
       *
       * Note that:
       *     - This effectively 'lock' main number keys to always output number events
       *       (except when using alt-gr).
       *     - This enforces users to use an ascii-compatible keymap with Blender -
       *       but at least it gives predictable and consistent results.
       *
       * Also, note that nothing in XLib sources [1] makes it obvious why those two functions give
       * different key_sym results...
       *
       * [1] http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/KeyBind.c
       */
      KeySym key_sym_str;
      /* Mode_switch 'modifier' is AltGr - when this one or Shift are enabled,
       * we do not want to apply that 'forced number' hack. */
      const unsigned int mode_switch_mask = XkbKeysymToModifiers(xke->display, XK_Mode_switch);
      const unsigned int number_hack_forbidden_kmods_mask = mode_switch_mask | ShiftMask;
      if ((xke->keycode >= 10 && xke->keycode < 20) &&
          ((xke->state & number_hack_forbidden_kmods_mask) == 0)) {
        key_sym = XLookupKeysym(xke, ShiftMask);
        if (!((key_sym >= XK_0) && (key_sym <= XK_9))) {
          key_sym = XLookupKeysym(xke, 0);
        }
      }
      else {
        key_sym = XLookupKeysym(xke, 0);
      }

      if (!XLookupString(xke, &ascii, 1, &key_sym_str, NULL)) {
        ascii = '\0';
      }

      /* Only allow a limited set of keys from XLookupKeysym,
       * all others we take from XLookupString, unless it gives unknown key... */
      gkey = ghost_key_from_keysym_or_keycode(key_sym, m_xkb_descr, xke->keycode);
      switch (gkey) {
        case GHOST_kKeyRightAlt:
        case GHOST_kKeyLeftAlt:
        case GHOST_kKeyRightShift:
        case GHOST_kKeyLeftShift:
        case GHOST_kKeyRightControl:
        case GHOST_kKeyLeftControl:
        case GHOST_kKeyOS:
        case GHOST_kKey0:
        case GHOST_kKey1:
        case GHOST_kKey2:
        case GHOST_kKey3:
        case GHOST_kKey4:
        case GHOST_kKey5:
        case GHOST_kKey6:
        case GHOST_kKey7:
        case GHOST_kKey8:
        case GHOST_kKey9:
        case GHOST_kKeyNumpad0:
        case GHOST_kKeyNumpad1:
        case GHOST_kKeyNumpad2:
        case GHOST_kKeyNumpad3:
        case GHOST_kKeyNumpad4:
        case GHOST_kKeyNumpad5:
        case GHOST_kKeyNumpad6:
        case GHOST_kKeyNumpad7:
        case GHOST_kKeyNumpad8:
        case GHOST_kKeyNumpad9:
        case GHOST_kKeyNumpadPeriod:
        case GHOST_kKeyNumpadEnter:
        case GHOST_kKeyNumpadPlus:
        case GHOST_kKeyNumpadMinus:
        case GHOST_kKeyNumpadAsterisk:
        case GHOST_kKeyNumpadSlash:
          break;
        default: {
          GHOST_TKey gkey_str = ghost_key_from_keysym(key_sym_str);
          if (gkey_str != GHOST_kKeyUnknown) {
            gkey = gkey_str;
          }
        }
      }
#else
      /* In keyboards like latin ones,
       * numbers needs a 'Shift' to be accessed but key_sym
       * is unmodified (or anyone swapping the keys with xmodmap).
       *
       * Here we look at the 'Shifted' version of the key.
       * If it is a number, then we take it instead of the normal key.
       *
       * The modified key is sent in the 'ascii's variable anyway.
       */
      if ((xke->keycode >= 10 && xke->keycode < 20) &&
          ((key_sym = XLookupKeysym(xke, ShiftMask)) >= XK_0) && (key_sym <= XK_9)) {
        /* pass (keep shift'ed key_sym) */
      }
      else {
        /* regular case */
        key_sym = XLookupKeysym(xke, 0);
      }

      gkey = ghost_key_from_keysym_or_keycode(key_sym, m_xkb_descr, xke->keycode);

      if (!XLookupString(xke, &ascii, 1, NULL, NULL)) {
        ascii = '\0';
      }
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* getting unicode on key-up events gives XLookupNone status */
      XIC xic = window->getX11_XIC();
      if (xic && xke->type == KeyPress) {
        Status status;

        /* use utf8 because its not locale depentant, from xorg docs */
        if (!(len = Xutf8LookupString(
                  xic, xke, utf8_buf, sizeof(utf8_array) - 5, &key_sym, &status))) {
          utf8_buf[0] = '\0';
        }

        if (status == XBufferOverflow) {
          utf8_buf = (char *)malloc(len + 5);
          len = Xutf8LookupString(xic, xke, utf8_buf, len, &key_sym, &status);
        }

        if ((status == XLookupChars || status == XLookupBoth)) {
          if ((unsigned char)utf8_buf[0] >= 32) { /* not an ascii control character */
            /* do nothing for now, this is valid utf8 */
          }
          else {
            utf8_buf[0] = '\0';
          }
        }
        else if (status == XLookupKeySym) {
          /* this key doesn't have a text representation, it is a command
           * key of some sort */
        }
        else {
          printf("Bad keycode lookup. Keysym 0x%x Status: %s\n",
                 (unsigned int)key_sym,
                 (status == XLookupNone ?
                      "XLookupNone" :
                      status == XLookupKeySym ? "XLookupKeySym" : "Unknown status"));

          printf("'%.*s' %p %p\n", len, utf8_buf, xic, m_xim);
        }
      }
      else {
        utf8_buf[0] = '\0';
      }
#endif

      g_event = new GHOST_EventKey(getMilliSeconds(), type, window, gkey, ascii, utf8_buf);

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* when using IM for some languages such as Japanese,
       * one event inserts multiple utf8 characters */
      if (xic && xke->type == KeyPress) {
        unsigned char c;
        int i = 0;
        while (1) {
          /* search character boundary */
          if ((unsigned char)utf8_buf[i++] > 0x7f) {
            for (; i < len; ++i) {
              c = utf8_buf[i];
              if (c < 0x80 || c > 0xbf)
                break;
            }
          }

          if (i >= len)
            break;

          /* enqueue previous character */
          pushEvent(g_event);

          g_event = new GHOST_EventKey(getMilliSeconds(), type, window, gkey, '\0', &utf8_buf[i]);
        }
      }

      if (utf8_buf != utf8_array)
        free(utf8_buf);
#endif

      break;
    }

    case ButtonPress:
    case ButtonRelease: {
      XButtonEvent &xbe = xe->xbutton;
      GHOST_TButtonMask gbmask = GHOST_kButtonMaskLeft;
      GHOST_TEventType type = (xbe.type == ButtonPress) ? GHOST_kEventButtonDown :
                                                          GHOST_kEventButtonUp;

      /* process wheel mouse events and break, only pass on press events */
      if (xbe.button == Button4) {
        if (xbe.type == ButtonPress)
          g_event = new GHOST_EventWheel(getMilliSeconds(), window, 1);
        break;
      }
      else if (xbe.button == Button5) {
        if (xbe.type == ButtonPress)
          g_event = new GHOST_EventWheel(getMilliSeconds(), window, -1);
        break;
      }

      /* process rest of normal mouse buttons */
      if (xbe.button == Button1)
        gbmask = GHOST_kButtonMaskLeft;
      else if (xbe.button == Button2)
        gbmask = GHOST_kButtonMaskMiddle;
      else if (xbe.button == Button3)
        gbmask = GHOST_kButtonMaskRight;
      /* It seems events 6 and 7 are for horizontal scrolling.
       * you can re-order button mapping like this... (swaps 6,7 with 8,9)
       *   xmodmap -e "pointer = 1 2 3 4 5 8 9 6 7"
       */
      else if (xbe.button == 6)
        gbmask = GHOST_kButtonMaskButton6;
      else if (xbe.button == 7)
        gbmask = GHOST_kButtonMaskButton7;
      else if (xbe.button == 8)
        gbmask = GHOST_kButtonMaskButton4;
      else if (xbe.button == 9)
        gbmask = GHOST_kButtonMaskButton5;
      else
        break;

      g_event = new GHOST_EventButton(getMilliSeconds(), type, window, gbmask);
      break;
    }

    /* change of size, border, layer etc. */
    case ConfigureNotify: {
      /* XConfigureEvent & xce = xe->xconfigure; */

      g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window);
      break;
    }

    case FocusIn:
    case FocusOut: {
      XFocusChangeEvent &xfe = xe->xfocus;

      /* TODO: make sure this is the correct place for activate/deactivate */
      // printf("X: focus %s for window %d\n",
      //        xfe.type == FocusIn ? "in" : "out", (int) xfe.window);

      /* May have to look at the type of event and filter some out. */

      GHOST_TEventType gtype = (xfe.type == FocusIn) ? GHOST_kEventWindowActivate :
                                                       GHOST_kEventWindowDeactivate;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      XIC xic = window->getX11_XIC();
      if (xic) {
        if (xe->type == FocusIn)
          XSetICFocus(xic);
        else
          XUnsetICFocus(xic);
      }
#endif

      g_event = new GHOST_Event(getMilliSeconds(), gtype, window);
      break;
    }
    case ClientMessage: {
      XClientMessageEvent &xcme = xe->xclient;

      if (((Atom)xcme.data.l[0]) == m_atom.WM_DELETE_WINDOW) {
        g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window);
      }
      else if (((Atom)xcme.data.l[0]) == m_atom.WM_TAKE_FOCUS) {
        XWindowAttributes attr;
        Window fwin;
        int revert_to;

        /* as ICCCM say, we need reply this event
         * with a SetInputFocus, the data[1] have
         * the valid timestamp (send by the wm).
         *
         * Some WM send this event before the
         * window is really mapped (for example
         * change from virtual desktop), so we need
         * to be sure that our windows is mapped
         * or this call fail and close blender.
         */
        if (XGetWindowAttributes(m_display, xcme.window, &attr) == True) {
          if (XGetInputFocus(m_display, &fwin, &revert_to) == True) {
            if (attr.map_state == IsViewable) {
              if (fwin != xcme.window)
                XSetInputFocus(m_display, xcme.window, RevertToParent, xcme.data.l[1]);
            }
          }
        }
      }
      else {
#ifdef WITH_XDND
        /* try to handle drag event
         * (if there's no such events, GHOST_HandleClientMessage will return zero) */
        if (window->getDropTarget()->GHOST_HandleClientMessage(xe) == false) {
          /* Unknown client message, ignore */
        }
#else
        /* Unknown client message, ignore */
#endif
      }

      break;
    }

    case DestroyNotify:
      ::exit(-1);
    /* We're not interested in the following things.(yet...) */
    case NoExpose:
    case GraphicsExpose:
      break;

    case EnterNotify:
    case LeaveNotify: {
      /* XCrossingEvents pointer leave enter window.
       * also do cursor move here, MotionNotify only
       * happens when motion starts & ends inside window.
       * we only do moves when the crossing mode is 'normal'
       * (really crossing between windows) since some windowmanagers
       * also send grab/ungrab crossings for mousewheel events.
       */
      XCrossingEvent &xce = xe->xcrossing;
      if (xce.mode == NotifyNormal) {
        g_event = new GHOST_EventCursor(
            getMilliSeconds(), GHOST_kEventCursorMove, window, xce.x_root, xce.y_root);
      }

      // printf("X: %s window %d\n",
      //        xce.type == EnterNotify ? "entering" : "leaving", (int) xce.window);

      if (xce.type == EnterNotify)
        m_windowManager->setActiveWindow(window);
      else
        m_windowManager->setWindowInactive(window);

      break;
    }
    case MapNotify:
      /*
       * From ICCCM:
       * [ Clients can select for StructureNotify on their
       *   top-level windows to track transition between
       *   Normal and Iconic states. Receipt of a MapNotify
       *   event will indicate a transition to the Normal
       *   state, and receipt of an UnmapNotify event will
       *   indicate a transition to the Iconic state. ]
       */
      if (window->m_post_init == True) {
        /*
         * Now we are sure that the window is
         * mapped, so only need change the state.
         */
        window->setState(window->m_post_state);
        window->m_post_init = False;
      }
      break;
    case UnmapNotify:
      break;
    case MappingNotify:
    case ReparentNotify:
      break;
    case SelectionRequest: {
      XEvent nxe;
      Atom target, utf8_string, string, compound_text, c_string;
      XSelectionRequestEvent *xse = &xe->xselectionrequest;

      target = XInternAtom(m_display, "TARGETS", False);
      utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
      string = XInternAtom(m_display, "STRING", False);
      compound_text = XInternAtom(m_display, "COMPOUND_TEXT", False);
      c_string = XInternAtom(m_display, "C_STRING", False);

      /* support obsolete clients */
      if (xse->property == None) {
        xse->property = xse->target;
      }

      nxe.xselection.type = SelectionNotify;
      nxe.xselection.requestor = xse->requestor;
      nxe.xselection.property = xse->property;
      nxe.xselection.display = xse->display;
      nxe.xselection.selection = xse->selection;
      nxe.xselection.target = xse->target;
      nxe.xselection.time = xse->time;

      /* Check to see if the requestor is asking for String */
      if (xse->target == utf8_string || xse->target == string || xse->target == compound_text ||
          xse->target == c_string) {
        if (xse->selection == XInternAtom(m_display, "PRIMARY", False)) {
          XChangeProperty(m_display,
                          xse->requestor,
                          xse->property,
                          xse->target,
                          8,
                          PropModeReplace,
                          (unsigned char *)txt_select_buffer,
                          strlen(txt_select_buffer));
        }
        else if (xse->selection == XInternAtom(m_display, "CLIPBOARD", False)) {
          XChangeProperty(m_display,
                          xse->requestor,
                          xse->property,
                          xse->target,
                          8,
                          PropModeReplace,
                          (unsigned char *)txt_cut_buffer,
                          strlen(txt_cut_buffer));
        }
      }
      else if (xse->target == target) {
        Atom alist[5];
        alist[0] = target;
        alist[1] = utf8_string;
        alist[2] = string;
        alist[3] = compound_text;
        alist[4] = c_string;
        XChangeProperty(m_display,
                        xse->requestor,
                        xse->property,
                        xse->target,
                        32,
                        PropModeReplace,
                        (unsigned char *)alist,
                        5);
        XFlush(m_display);
      }
      else {
        /* Change property to None because we do not support anything but STRING */
        nxe.xselection.property = None;
      }

      /* Send the event to the client 0 0 == False, SelectionNotify */
      XSendEvent(m_display, xse->requestor, 0, 0, &nxe);
      XFlush(m_display);
      break;
    }

    default: {
#ifdef WITH_X11_XINPUT
      for (GHOST_TabletX11 &xtablet : m_xtablets) {
        if (xe->type == xtablet.MotionEvent || xe->type == xtablet.PressEvent) {
          XDeviceMotionEvent *data = (XDeviceMotionEvent *)xe;
          if (data->deviceid != xtablet.ID) {
            continue;
          }

          const unsigned char axis_first = data->first_axis;
          const unsigned char axes_end = axis_first + data->axes_count; /* after the last */
          int axis_value;

          /* stroke might begin without leading ProxyIn event,
           * this happens when window is opened when stylus is already hovering
           * around tablet surface */
          window->GetTabletData()->Active = xtablet.mode;

          /* Note: This event might be generated with incomplete dataset
           * (don't exactly know why, looks like in some cases, if the value does not change,
           * it is not included in subsequent XDeviceMotionEvent events).
           * So we have to check which values this event actually contains!
           */

#  define AXIS_VALUE_GET(axis, val) \
    ((axis_first <= axis && axes_end > axis) && \
     ((void)(val = data->axis_data[axis - axis_first]), true))

          if (AXIS_VALUE_GET(2, axis_value)) {
            window->GetTabletData()->Pressure = axis_value / ((float)xtablet.PressureLevels);
          }

          /* the (short) cast and the & 0xffff is bizarre and unexplained anywhere,
           * but I got garbage data without it. Found it in the xidump.c source --matt
           *
           * The '& 0xffff' just truncates the value to its two lowest bytes, this probably means
           * some drivers do not properly set the whole int value? Since we convert to float
           * afterward, I don't think we need to cast to short here, but do not have a device to
           * check this. --mont29
           */
          if (AXIS_VALUE_GET(3, axis_value)) {
            window->GetTabletData()->Xtilt = (short)(axis_value & 0xffff) /
                                             ((float)xtablet.XtiltLevels);
          }
          if (AXIS_VALUE_GET(4, axis_value)) {
            window->GetTabletData()->Ytilt = (short)(axis_value & 0xffff) /
                                             ((float)xtablet.YtiltLevels);
          }

#  undef AXIS_VALUE_GET
        }
        else if (xe->type == xtablet.ProxInEvent) {
          XProximityNotifyEvent *data = (XProximityNotifyEvent *)xe;
          if (data->deviceid != xtablet.ID) {
            continue;
          }

          window->GetTabletData()->Active = xtablet.mode;
        }
        else if (xe->type == xtablet.ProxOutEvent) {
          window->GetTabletData()->Active = GHOST_kTabletModeNone;
        }
      }
#endif  // WITH_X11_XINPUT
      break;
    }
  }

  if (g_event) {
    pushEvent(g_event);
  }
}

GHOST_TSuccess GHOST_SystemX11::getModifierKeys(GHOST_ModifierKeys &keys) const
{

  /* analyse the masks retuned from XQueryPointer. */

  memset((void *)m_keyboard_vector, 0, sizeof(m_keyboard_vector));

  XQueryKeymap(m_display, (char *)m_keyboard_vector);

  /* now translate key symbols into keycodes and
   * test with vector. */

  const static KeyCode shift_l = XKeysymToKeycode(m_display, XK_Shift_L);
  const static KeyCode shift_r = XKeysymToKeycode(m_display, XK_Shift_R);
  const static KeyCode control_l = XKeysymToKeycode(m_display, XK_Control_L);
  const static KeyCode control_r = XKeysymToKeycode(m_display, XK_Control_R);
  const static KeyCode alt_l = XKeysymToKeycode(m_display, XK_Alt_L);
  const static KeyCode alt_r = XKeysymToKeycode(m_display, XK_Alt_R);
  const static KeyCode super_l = XKeysymToKeycode(m_display, XK_Super_L);
  const static KeyCode super_r = XKeysymToKeycode(m_display, XK_Super_R);

  /* shift */
  keys.set(GHOST_kModifierKeyLeftShift,
           ((m_keyboard_vector[shift_l >> 3] >> (shift_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightShift,
           ((m_keyboard_vector[shift_r >> 3] >> (shift_r & 7)) & 1) != 0);
  /* control */
  keys.set(GHOST_kModifierKeyLeftControl,
           ((m_keyboard_vector[control_l >> 3] >> (control_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightControl,
           ((m_keyboard_vector[control_r >> 3] >> (control_r & 7)) & 1) != 0);
  /* alt */
  keys.set(GHOST_kModifierKeyLeftAlt, ((m_keyboard_vector[alt_l >> 3] >> (alt_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightAlt, ((m_keyboard_vector[alt_r >> 3] >> (alt_r & 7)) & 1) != 0);
  /* super (windows) - only one GHOST-kModifierKeyOS, so mapping to either */
  keys.set(GHOST_kModifierKeyOS,
           (((m_keyboard_vector[super_l >> 3] >> (super_l & 7)) & 1) ||
            ((m_keyboard_vector[super_r >> 3] >> (super_r & 7)) & 1)) != 0);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemX11::getButtons(GHOST_Buttons &buttons) const
{
  Window root_return, child_return;
  int rx, ry, wx, wy;
  unsigned int mask_return;

  if (XQueryPointer(m_display,
                    RootWindow(m_display, DefaultScreen(m_display)),
                    &root_return,
                    &child_return,
                    &rx,
                    &ry,
                    &wx,
                    &wy,
                    &mask_return) == True) {
    buttons.set(GHOST_kButtonMaskLeft, (mask_return & Button1Mask) != 0);
    buttons.set(GHOST_kButtonMaskMiddle, (mask_return & Button2Mask) != 0);
    buttons.set(GHOST_kButtonMaskRight, (mask_return & Button3Mask) != 0);
  }
  else {
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

static GHOST_TSuccess getCursorPosition_impl(Display *display,
                                             GHOST_TInt32 &x,
                                             GHOST_TInt32 &y,
                                             Window *child_return)
{
  int rx, ry, wx, wy;
  unsigned int mask_return;
  Window root_return;

  if (XQueryPointer(display,
                    RootWindow(display, DefaultScreen(display)),
                    &root_return,
                    child_return,
                    &rx,
                    &ry,
                    &wx,
                    &wy,
                    &mask_return) == False) {
    return GHOST_kFailure;
  }
  else {
    x = rx;
    y = ry;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemX11::getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
  Window child_return;
  return getCursorPosition_impl(m_display, x, y, &child_return);
}

GHOST_TSuccess GHOST_SystemX11::setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y)
{

  /* This is a brute force move in screen coordinates
   * XWarpPointer does relative moves so first determine the
   * current pointer position. */

  int cx, cy;

#ifdef WITH_XWAYLAND_HACK
  Window child_return = None;
  if (getCursorPosition_impl(m_display, cx, cy, &child_return) == GHOST_kFailure) {
    return GHOST_kFailure;
  }
#else
  if (getCursorPosition(cx, cy) == GHOST_kFailure) {
    return GHOST_kFailure;
  }
#endif

  int relx = x - cx;
  int rely = y - cy;

#ifdef WITH_XWAYLAND_HACK
  if (use_xwayland_hack) {
    if (child_return != None) {
      XFixesHideCursor(m_display, child_return);
    }
  }
#endif

#if defined(WITH_X11_XINPUT) && defined(USE_X11_XINPUT_WARP)
  if ((m_xinput_version.present) && (m_xinput_version.major_version >= 2)) {
    /* Needed to account for XInput "Coordinate Transformation Matrix", see T48901 */
    int device_id;
    if (XIGetClientPointer(m_display, None, &device_id) != False) {
      XIWarpPointer(m_display, device_id, None, None, 0, 0, 0, 0, relx, rely);
    }
  }
  else
#endif
  {
    XWarpPointer(m_display, None, None, 0, 0, 0, 0, relx, rely);
  }

#ifdef WITH_XWAYLAND_HACK
  if (use_xwayland_hack) {
    if (child_return != None) {
      XFixesShowCursor(m_display, child_return);
    }
  }
#endif

  XSync(m_display, 0); /* Sync to process all requests */

  return GHOST_kSuccess;
}

void GHOST_SystemX11::addDirtyWindow(GHOST_WindowX11 *bad_wind)
{
  GHOST_ASSERT((bad_wind != NULL), "addDirtyWindow() NULL ptr trapped (window)");

  m_dirty_windows.push_back(bad_wind);
}

bool GHOST_SystemX11::generateWindowExposeEvents()
{
  vector<GHOST_WindowX11 *>::iterator w_start = m_dirty_windows.begin();
  vector<GHOST_WindowX11 *>::const_iterator w_end = m_dirty_windows.end();
  bool anyProcessed = false;

  for (; w_start != w_end; ++w_start) {
    GHOST_Event *g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, *w_start);

    (*w_start)->validate();

    if (g_event) {
      pushEvent(g_event);
      anyProcessed = true;
    }
  }

  m_dirty_windows.clear();
  return anyProcessed;
}

static GHOST_TKey ghost_key_from_keysym_or_keycode(const KeySym keysym,
                                                   XkbDescPtr xkb_descr,
                                                   const KeyCode keycode)
{
  GHOST_TKey type = ghost_key_from_keysym(keysym);
  if (type == GHOST_kKeyUnknown) {
    if (xkb_descr) {
      type = ghost_key_from_keycode(xkb_descr, keycode);
    }
  }
  return type;
}

#define GXMAP(k, x, y) \
  case x: \
    k = y; \
    break

static GHOST_TKey ghost_key_from_keysym(const KeySym key)
{
  GHOST_TKey type;

  if ((key >= XK_A) && (key <= XK_Z)) {
    type = GHOST_TKey(key - XK_A + int(GHOST_kKeyA));
  }
  else if ((key >= XK_a) && (key <= XK_z)) {
    type = GHOST_TKey(key - XK_a + int(GHOST_kKeyA));
  }
  else if ((key >= XK_0) && (key <= XK_9)) {
    type = GHOST_TKey(key - XK_0 + int(GHOST_kKey0));
  }
  else if ((key >= XK_F1) && (key <= XK_F24)) {
    type = GHOST_TKey(key - XK_F1 + int(GHOST_kKeyF1));
  }
  else {
    switch (key) {
      GXMAP(type, XK_BackSpace, GHOST_kKeyBackSpace);
      GXMAP(type, XK_Tab, GHOST_kKeyTab);
      GXMAP(type, XK_ISO_Left_Tab, GHOST_kKeyTab);
      GXMAP(type, XK_Return, GHOST_kKeyEnter);
      GXMAP(type, XK_Escape, GHOST_kKeyEsc);
      GXMAP(type, XK_space, GHOST_kKeySpace);

      GXMAP(type, XK_Linefeed, GHOST_kKeyLinefeed);
      GXMAP(type, XK_semicolon, GHOST_kKeySemicolon);
      GXMAP(type, XK_period, GHOST_kKeyPeriod);
      GXMAP(type, XK_comma, GHOST_kKeyComma);
      GXMAP(type, XK_quoteright, GHOST_kKeyQuote);
      GXMAP(type, XK_quoteleft, GHOST_kKeyAccentGrave);
      GXMAP(type, XK_minus, GHOST_kKeyMinus);
      GXMAP(type, XK_plus, GHOST_kKeyPlus);
      GXMAP(type, XK_slash, GHOST_kKeySlash);
      GXMAP(type, XK_backslash, GHOST_kKeyBackslash);
      GXMAP(type, XK_equal, GHOST_kKeyEqual);
      GXMAP(type, XK_bracketleft, GHOST_kKeyLeftBracket);
      GXMAP(type, XK_bracketright, GHOST_kKeyRightBracket);
      GXMAP(type, XK_Pause, GHOST_kKeyPause);

      GXMAP(type, XK_Shift_L, GHOST_kKeyLeftShift);
      GXMAP(type, XK_Shift_R, GHOST_kKeyRightShift);
      GXMAP(type, XK_Control_L, GHOST_kKeyLeftControl);
      GXMAP(type, XK_Control_R, GHOST_kKeyRightControl);
      GXMAP(type, XK_Alt_L, GHOST_kKeyLeftAlt);
      GXMAP(type, XK_Alt_R, GHOST_kKeyRightAlt);
      GXMAP(type, XK_Super_L, GHOST_kKeyOS);
      GXMAP(type, XK_Super_R, GHOST_kKeyOS);

      GXMAP(type, XK_Insert, GHOST_kKeyInsert);
      GXMAP(type, XK_Delete, GHOST_kKeyDelete);
      GXMAP(type, XK_Home, GHOST_kKeyHome);
      GXMAP(type, XK_End, GHOST_kKeyEnd);
      GXMAP(type, XK_Page_Up, GHOST_kKeyUpPage);
      GXMAP(type, XK_Page_Down, GHOST_kKeyDownPage);

      GXMAP(type, XK_Left, GHOST_kKeyLeftArrow);
      GXMAP(type, XK_Right, GHOST_kKeyRightArrow);
      GXMAP(type, XK_Up, GHOST_kKeyUpArrow);
      GXMAP(type, XK_Down, GHOST_kKeyDownArrow);

      GXMAP(type, XK_Caps_Lock, GHOST_kKeyCapsLock);
      GXMAP(type, XK_Scroll_Lock, GHOST_kKeyScrollLock);
      GXMAP(type, XK_Num_Lock, GHOST_kKeyNumLock);

      /* keypad events */

      GXMAP(type, XK_KP_0, GHOST_kKeyNumpad0);
      GXMAP(type, XK_KP_1, GHOST_kKeyNumpad1);
      GXMAP(type, XK_KP_2, GHOST_kKeyNumpad2);
      GXMAP(type, XK_KP_3, GHOST_kKeyNumpad3);
      GXMAP(type, XK_KP_4, GHOST_kKeyNumpad4);
      GXMAP(type, XK_KP_5, GHOST_kKeyNumpad5);
      GXMAP(type, XK_KP_6, GHOST_kKeyNumpad6);
      GXMAP(type, XK_KP_7, GHOST_kKeyNumpad7);
      GXMAP(type, XK_KP_8, GHOST_kKeyNumpad8);
      GXMAP(type, XK_KP_9, GHOST_kKeyNumpad9);
      GXMAP(type, XK_KP_Decimal, GHOST_kKeyNumpadPeriod);

      GXMAP(type, XK_KP_Insert, GHOST_kKeyNumpad0);
      GXMAP(type, XK_KP_End, GHOST_kKeyNumpad1);
      GXMAP(type, XK_KP_Down, GHOST_kKeyNumpad2);
      GXMAP(type, XK_KP_Page_Down, GHOST_kKeyNumpad3);
      GXMAP(type, XK_KP_Left, GHOST_kKeyNumpad4);
      GXMAP(type, XK_KP_Begin, GHOST_kKeyNumpad5);
      GXMAP(type, XK_KP_Right, GHOST_kKeyNumpad6);
      GXMAP(type, XK_KP_Home, GHOST_kKeyNumpad7);
      GXMAP(type, XK_KP_Up, GHOST_kKeyNumpad8);
      GXMAP(type, XK_KP_Page_Up, GHOST_kKeyNumpad9);
      GXMAP(type, XK_KP_Delete, GHOST_kKeyNumpadPeriod);

      GXMAP(type, XK_KP_Enter, GHOST_kKeyNumpadEnter);
      GXMAP(type, XK_KP_Add, GHOST_kKeyNumpadPlus);
      GXMAP(type, XK_KP_Subtract, GHOST_kKeyNumpadMinus);
      GXMAP(type, XK_KP_Multiply, GHOST_kKeyNumpadAsterisk);
      GXMAP(type, XK_KP_Divide, GHOST_kKeyNumpadSlash);

      /* Media keys in some keyboards and laptops with XFree86/Xorg */
#ifdef WITH_XF86KEYSYM
      GXMAP(type, XF86XK_AudioPlay, GHOST_kKeyMediaPlay);
      GXMAP(type, XF86XK_AudioStop, GHOST_kKeyMediaStop);
      GXMAP(type, XF86XK_AudioPrev, GHOST_kKeyMediaFirst);
      GXMAP(type, XF86XK_AudioRewind, GHOST_kKeyMediaFirst);
      GXMAP(type, XF86XK_AudioNext, GHOST_kKeyMediaLast);
#  ifdef XF86XK_AudioForward /* Debian lenny's XF86keysym.h has no XF86XK_AudioForward define */
      GXMAP(type, XF86XK_AudioForward, GHOST_kKeyMediaLast);
#  endif
#endif
      default:
#ifdef GHOST_DEBUG
        printf("%s: unknown key: %lu / 0x%lx\n", __func__, key, key);
#endif
        type = GHOST_kKeyUnknown;
        break;
    }
  }

  return type;
}

#undef GXMAP

#define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))

static GHOST_TKey ghost_key_from_keycode(const XkbDescPtr xkb_descr, const KeyCode keycode)
{
  GHOST_ASSERT(XkbKeyNameLength == 4, "Name length is invalid!");
  if (keycode >= xkb_descr->min_key_code && keycode <= xkb_descr->max_key_code) {
    const char *id_str = xkb_descr->names->keys[keycode].name;
    const uint32_t id = MAKE_ID(id_str[0], id_str[1], id_str[2], id_str[3]);
    switch (id) {
      case MAKE_ID('T', 'L', 'D', 'E'):
        return GHOST_kKeyAccentGrave;
#ifdef GHOST_DEBUG
      default:
        printf("%s unhandled keycode: %.*s\n", __func__, XkbKeyNameLength, id_str);
        break;
#endif
    }
  }
  else if (keycode != 0) {
    GHOST_ASSERT(false, "KeyCode out of range!");
  }
  return GHOST_kKeyUnknown;
}

#undef MAKE_ID

/* from xclip.c xcout() v0.11 */

#define XCLIB_XCOUT_NONE 0          /* no context */
#define XCLIB_XCOUT_SENTCONVSEL 1   /* sent a request */
#define XCLIB_XCOUT_INCR 2          /* in an incr loop */
#define XCLIB_XCOUT_FALLBACK 3      /* STRING failed, need fallback to UTF8 */
#define XCLIB_XCOUT_FALLBACK_UTF8 4 /* UTF8 failed, move to compouned */
#define XCLIB_XCOUT_FALLBACK_COMP 5 /* compouned failed, move to text. */
#define XCLIB_XCOUT_FALLBACK_TEXT 6

/* Retrieves the contents of a selections. */
void GHOST_SystemX11::getClipboard_xcout(const XEvent *evt,
                                         Atom sel,
                                         Atom target,
                                         unsigned char **txt,
                                         unsigned long *len,
                                         unsigned int *context) const
{
  Atom pty_type;
  int pty_format;
  unsigned char *buffer;
  unsigned long pty_size, pty_items;
  unsigned char *ltxt = *txt;

  vector<GHOST_IWindow *> &win_vec = m_windowManager->getWindows();
  vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  Window win = window->getXWindow();

  switch (*context) {
    /* There is no context, do an XConvertSelection() */
    case XCLIB_XCOUT_NONE:
      /* Initialise return length to 0 */
      if (*len > 0) {
        free(*txt);
        *len = 0;
      }

      /* Send a selection request */
      XConvertSelection(m_display, sel, target, m_atom.XCLIP_OUT, win, CurrentTime);
      *context = XCLIB_XCOUT_SENTCONVSEL;
      return;

    case XCLIB_XCOUT_SENTCONVSEL:
      if (evt->type != SelectionNotify)
        return;

      if (target == m_atom.UTF8_STRING && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_UTF8;
        return;
      }
      else if (target == m_atom.COMPOUND_TEXT && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_COMP;
        return;
      }
      else if (target == m_atom.TEXT && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_TEXT;
        return;
      }

      /* find the size and format of the data in property */
      XGetWindowProperty(m_display,
                         win,
                         m_atom.XCLIP_OUT,
                         0,
                         0,
                         False,
                         AnyPropertyType,
                         &pty_type,
                         &pty_format,
                         &pty_items,
                         &pty_size,
                         &buffer);
      XFree(buffer);

      if (pty_type == m_atom.INCR) {
        /* start INCR mechanism by deleting property */
        XDeleteProperty(m_display, win, m_atom.XCLIP_OUT);
        XFlush(m_display);
        *context = XCLIB_XCOUT_INCR;
        return;
      }

      /* if it's not incr, and not format == 8, then there's
       * nothing in the selection (that xclip understands, anyway) */

      if (pty_format != 8) {
        *context = XCLIB_XCOUT_NONE;
        return;
      }

      // not using INCR mechanism, just read the property
      XGetWindowProperty(m_display,
                         win,
                         m_atom.XCLIP_OUT,
                         0,
                         (long)pty_size,
                         False,
                         AnyPropertyType,
                         &pty_type,
                         &pty_format,
                         &pty_items,
                         &pty_size,
                         &buffer);

      /* finished with property, delete it */
      XDeleteProperty(m_display, win, m_atom.XCLIP_OUT);

      /* copy the buffer to the pointer for returned data */
      ltxt = (unsigned char *)malloc(pty_items);
      memcpy(ltxt, buffer, pty_items);

      /* set the length of the returned data */
      *len = pty_items;
      *txt = ltxt;

      /* free the buffer */
      XFree(buffer);

      *context = XCLIB_XCOUT_NONE;

      /* complete contents of selection fetched, return 1 */
      return;

    case XCLIB_XCOUT_INCR:
      /* To use the INCR method, we basically delete the
       * property with the selection in it, wait for an
       * event indicating that the property has been created,
       * then read it, delete it, etc. */

      /* make sure that the event is relevant */
      if (evt->type != PropertyNotify)
        return;

      /* skip unless the property has a new value */
      if (evt->xproperty.state != PropertyNewValue)
        return;

      /* check size and format of the property */
      XGetWindowProperty(m_display,
                         win,
                         m_atom.XCLIP_OUT,
                         0,
                         0,
                         False,
                         AnyPropertyType,
                         &pty_type,
                         &pty_format,
                         &pty_items,
                         &pty_size,
                         &buffer);

      if (pty_format != 8) {
        /* property does not contain text, delete it
         * to tell the other X client that we have read
         * it and to send the next property */
        XFree(buffer);
        XDeleteProperty(m_display, win, m_atom.XCLIP_OUT);
        return;
      }

      if (pty_size == 0) {
        /* no more data, exit from loop */
        XFree(buffer);
        XDeleteProperty(m_display, win, m_atom.XCLIP_OUT);
        *context = XCLIB_XCOUT_NONE;

        /* this means that an INCR transfer is now
         * complete, return 1 */
        return;
      }

      XFree(buffer);

      /* if we have come this far, the property contains
       * text, we know the size. */
      XGetWindowProperty(m_display,
                         win,
                         m_atom.XCLIP_OUT,
                         0,
                         (long)pty_size,
                         False,
                         AnyPropertyType,
                         &pty_type,
                         &pty_format,
                         &pty_items,
                         &pty_size,
                         &buffer);

      /* allocate memory to accommodate data in *txt */
      if (*len == 0) {
        *len = pty_items;
        ltxt = (unsigned char *)malloc(*len);
      }
      else {
        *len += pty_items;
        ltxt = (unsigned char *)realloc(ltxt, *len);
      }

      /* add data to ltxt */
      memcpy(&ltxt[*len - pty_items], buffer, pty_items);

      *txt = ltxt;
      XFree(buffer);

      /* delete property to get the next item */
      XDeleteProperty(m_display, win, m_atom.XCLIP_OUT);
      XFlush(m_display);
      return;
  }
  return;
}

GHOST_TUns8 *GHOST_SystemX11::getClipboard(bool selection) const
{
  Atom sseln;
  Atom target = m_atom.UTF8_STRING;
  Window owner;

  /* from xclip.c doOut() v0.11 */
  unsigned char *sel_buf;
  unsigned long sel_len = 0;
  XEvent evt;
  unsigned int context = XCLIB_XCOUT_NONE;

  if (selection == True)
    sseln = m_atom.PRIMARY;
  else
    sseln = m_atom.CLIPBOARD;

  vector<GHOST_IWindow *> &win_vec = m_windowManager->getWindows();
  vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  Window win = window->getXWindow();

  /* check if we are the owner. */
  owner = XGetSelectionOwner(m_display, sseln);
  if (owner == win) {
    if (sseln == m_atom.CLIPBOARD) {
      sel_buf = (unsigned char *)malloc(strlen(txt_cut_buffer) + 1);
      strcpy((char *)sel_buf, txt_cut_buffer);
      return sel_buf;
    }
    else {
      sel_buf = (unsigned char *)malloc(strlen(txt_select_buffer) + 1);
      strcpy((char *)sel_buf, txt_select_buffer);
      return sel_buf;
    }
  }
  else if (owner == None)
    return (NULL);

  while (1) {
    /* only get an event if xcout() is doing something */
    if (context != XCLIB_XCOUT_NONE)
      XNextEvent(m_display, &evt);

    /* fetch the selection, or part of it */
    getClipboard_xcout(&evt, sseln, target, &sel_buf, &sel_len, &context);

    /* fallback is needed. set XA_STRING to target and restart the loop. */
    if (context == XCLIB_XCOUT_FALLBACK) {
      context = XCLIB_XCOUT_NONE;
      target = m_atom.STRING;
      continue;
    }
    else if (context == XCLIB_XCOUT_FALLBACK_UTF8) {
      /* utf8 fail, move to compouned text. */
      context = XCLIB_XCOUT_NONE;
      target = m_atom.COMPOUND_TEXT;
      continue;
    }
    else if (context == XCLIB_XCOUT_FALLBACK_COMP) {
      /* compouned text fail, move to text. */
      context = XCLIB_XCOUT_NONE;
      target = m_atom.TEXT;
      continue;
    }
    else if (context == XCLIB_XCOUT_FALLBACK_TEXT) {
      /* text fail, nothing else to try, break. */
      context = XCLIB_XCOUT_NONE;
    }

    /* only continue if xcout() is doing something */
    if (context == XCLIB_XCOUT_NONE)
      break;
  }

  if (sel_len) {
    /* only print the buffer out, and free it, if it's not
     * empty
     */
    unsigned char *tmp_data = (unsigned char *)malloc(sel_len + 1);
    memcpy((char *)tmp_data, (char *)sel_buf, sel_len);
    tmp_data[sel_len] = '\0';

    if (sseln == m_atom.STRING)
      XFree(sel_buf);
    else
      free(sel_buf);

    return tmp_data;
  }
  return (NULL);
}

void GHOST_SystemX11::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
  Window m_window, owner;

  vector<GHOST_IWindow *> &win_vec = m_windowManager->getWindows();
  vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  m_window = window->getXWindow();

  if (buffer) {
    if (selection == False) {
      XSetSelectionOwner(m_display, m_atom.CLIPBOARD, m_window, CurrentTime);
      owner = XGetSelectionOwner(m_display, m_atom.CLIPBOARD);
      if (txt_cut_buffer)
        free((void *)txt_cut_buffer);

      txt_cut_buffer = (char *)malloc(strlen(buffer) + 1);
      strcpy(txt_cut_buffer, buffer);
    }
    else {
      XSetSelectionOwner(m_display, m_atom.PRIMARY, m_window, CurrentTime);
      owner = XGetSelectionOwner(m_display, m_atom.PRIMARY);
      if (txt_select_buffer)
        free((void *)txt_select_buffer);

      txt_select_buffer = (char *)malloc(strlen(buffer) + 1);
      strcpy(txt_select_buffer, buffer);
    }

    if (owner != m_window)
      fprintf(stderr, "failed to own primary\n");
  }
}

#ifdef WITH_XDND
GHOST_TSuccess GHOST_SystemX11::pushDragDropEvent(GHOST_TEventType eventType,
                                                  GHOST_TDragnDropTypes draggedObjectType,
                                                  GHOST_IWindow *window,
                                                  int mouseX,
                                                  int mouseY,
                                                  void *data)
{
  GHOST_SystemX11 *system = ((GHOST_SystemX11 *)getSystem());
  return system->pushEvent(new GHOST_EventDragnDrop(
      system->getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, data));
}
#endif
/**
 * These callbacks can be used for debugging, so we can breakpoint on an X11 error.
 *
 * Dummy function to get around IO Handler exiting if device invalid
 * Basically it will not crash blender now if you have a X device that
 * is configured but not plugged in.
 */
int GHOST_X11_ApplicationErrorHandler(Display *display, XErrorEvent *event)
{
  char error_code_str[512];

  XGetErrorText(display, event->error_code, error_code_str, sizeof(error_code_str));

  fprintf(stderr,
          "Received X11 Error:\n"
          "\terror code:   %d\n"
          "\trequest code: %d\n"
          "\tminor code:   %d\n"
          "\terror text:   %s\n",
          event->error_code,
          event->request_code,
          event->minor_code,
          error_code_str);

  /* No exit! - but keep lint happy */
  return 0;
}

int GHOST_X11_ApplicationIOErrorHandler(Display * /*display*/)
{
  fprintf(stderr, "Ignoring Xlib error: error IO\n");

  /* No exit! - but keep lint happy */
  return 0;
}

#ifdef WITH_X11_XINPUT

/* These C functions are copied from Wine 3.12's wintab.c */
static bool match_token(const char *haystack, const char *needle)
{
  const char *p, *q;
  for (p = haystack; *p;) {
    while (*p && isspace(*p))
      p++;
    if (!*p)
      break;

    for (q = needle; *q && *p && tolower(*p) == tolower(*q); q++)
      p++;
    if (!*q && (isspace(*p) || !*p))
      return true;

    while (*p && !isspace(*p))
      p++;
  }
  return false;
}

/* Determining if an X device is a Tablet style device is an imperfect science.
 * We rely on common conventions around device names as well as the type reported
 * by Wacom tablets.  This code will likely need to be expanded for alternate tablet types
 *
 * Wintab refers to any device that interacts with the tablet as a cursor,
 * (stylus, eraser, tablet mouse, airbrush, etc)
 * this is not to be confused with wacom x11 configuration "cursor" device.
 * Wacoms x11 config "cursor" refers to its device slot (which we mirror with
 * our gSysCursors) for puck like devices (tablet mice essentially).
 */
static GHOST_TTabletMode tablet_mode_from_name(const char *name, const char *type)
{
  int i;
  static const char *tablet_stylus_whitelist[] = {"stylus", "wizardpen", "acecad", "pen", NULL};

  static const char *type_blacklist[] = {"pad", "cursor", "touch", NULL};

  /* Skip some known unsupported types. */
  for (i = 0; type_blacklist[i] != NULL; i++) {
    if (type && (strcasecmp(type, type_blacklist[i]) == 0)) {
      return GHOST_kTabletModeNone;
    }
  }

  /* First check device type to avoid cases where name is "Pen and Eraser" and type is "ERASER" */
  for (i = 0; tablet_stylus_whitelist[i] != NULL; i++) {
    if (type && match_token(type, tablet_stylus_whitelist[i])) {
      return GHOST_kTabletModeStylus;
    }
  }
  if (type && match_token(type, "eraser")) {
    return GHOST_kTabletModeEraser;
  }
  for (i = 0; tablet_stylus_whitelist[i] != NULL; i++) {
    if (name && match_token(name, tablet_stylus_whitelist[i])) {
      return GHOST_kTabletModeStylus;
    }
  }
  if (name && match_token(name, "eraser")) {
    return GHOST_kTabletModeEraser;
  }

  return GHOST_kTabletModeNone;
}

/* End code copied from Wine. */

void GHOST_SystemX11::refreshXInputDevices()
{
  if (m_xinput_version.present) {
    /* Close tablet devices. */
    clearXInputDevices();

    /* Install our error handler to override Xlib's termination behavior */
    GHOST_X11_ERROR_HANDLERS_OVERRIDE(handler_store);

    {
      int device_count;
      XDeviceInfo *device_info = XListInputDevices(m_display, &device_count);

      for (int i = 0; i < device_count; ++i) {
        char *device_type = device_info[i].type ? XGetAtomName(m_display, device_info[i].type) :
                                                  NULL;
        GHOST_TTabletMode tablet_mode = tablet_mode_from_name(device_info[i].name, device_type);

        // printf("Tablet type:'%s', name:'%s', index:%d\n", device_type, device_info[i].name, i);

        if (device_type) {
          XFree((void *)device_type);
        }

        if (!(tablet_mode == GHOST_kTabletModeStylus || tablet_mode == GHOST_kTabletModeEraser)) {
          continue;
        }

        GHOST_TabletX11 xtablet = {tablet_mode};
        xtablet.ID = device_info[i].id;
        xtablet.Device = XOpenDevice(m_display, xtablet.ID);

        if (xtablet.Device != NULL) {
          /* Find how many pressure levels tablet has */
          XAnyClassPtr ici = device_info[i].inputclassinfo;

          for (int j = 0; j < xtablet.Device->num_classes; ++j) {
            if (ici->c_class == ValuatorClass) {
              XValuatorInfo *xvi = (XValuatorInfo *)ici;
              xtablet.PressureLevels = xvi->axes[2].max_value;

              if (xvi->num_axes > 3) {
                /* this is assuming that the tablet has the same tilt resolution in both
                 * positive and negative directions. It would be rather weird if it didn't.. */
                xtablet.XtiltLevels = xvi->axes[3].max_value;
                xtablet.YtiltLevels = xvi->axes[4].max_value;
              }
              else {
                xtablet.XtiltLevels = 0;
                xtablet.YtiltLevels = 0;
              }

              break;
            }

            ici = (XAnyClassPtr)(((char *)ici) + ici->length);
          }

          m_xtablets.push_back(xtablet);
        }
      }

      XFreeDeviceList(device_info);
    }

    GHOST_X11_ERROR_HANDLERS_RESTORE(handler_store);
  }
}

void GHOST_SystemX11::clearXInputDevices()
{
  for (GHOST_TabletX11 &xtablet : m_xtablets) {
    if (xtablet.Device)
      XCloseDevice(m_display, xtablet.Device);
  }

  m_xtablets.clear();
}

#endif /* WITH_X11_XINPUT */
