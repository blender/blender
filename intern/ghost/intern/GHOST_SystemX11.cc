/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Part of this code from Nokia has been taken from Qt, under LGPL license. */

/** \file
 * \ingroup GHOST
 */

#include <X11/XKBlib.h> /* Allow detectable auto-repeat. */
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventWheel.hh"
#include "GHOST_SystemX11.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_WindowManager.hh"
#include "GHOST_WindowX11.hh"
#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerUnix.hh"
#endif
#include "GHOST_utildefines.hh"

#ifdef WITH_XDND
#  include "GHOST_DropTargetX11.hh"
#endif

#include "GHOST_Debug.hh"

#ifdef WITH_OPENGL_BACKEND
#  include "GHOST_ContextEGL.hh"
#  include "GHOST_ContextGLX.hh"
#endif

#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#ifdef WITH_XF86KEYSYM
#  include <X11/XF86keysym.h>
#endif

#ifdef WITH_X11_XFIXES
#  include <X11/extensions/Xfixes.h>
/* Workaround for XWayland grab glitch: #53004. */
#  define WITH_XWAYLAND_HACK
#endif

/* for XIWarpPointer */
#ifdef WITH_X11_XINPUT
#  include <X11/extensions/XInput2.h>
#endif

/* For timing */
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>  /* for fprintf only */
#include <cstdlib> /* for exit */
#include <iostream>
#include <vector>

/* For debugging, so we can break-point X11 errors. */
// #define USE_X11_ERROR_HANDLERS

#ifdef WITH_X11_XINPUT
#  define USE_XINPUT_HOTPLUG
#endif

/* see #34039 Fix Alt key glitch on Unity desktop */
#define USE_UNITY_WORKAROUND

/* Fix 'shortcut' part of keyboard reading code only ever using first defined key-map
 * instead of active one. See #47228 and D1746 */
#define USE_NON_LATIN_KB_WORKAROUND

static uchar bit_is_on(const uchar *ptr, int bit)
{
  return ptr[bit >> 3] & (1 << (bit & 7));
}

static GHOST_TKey ghost_key_from_keysym(const KeySym key);
static GHOST_TKey ghost_key_from_keycode(const XkbDescPtr xkb_descr, const KeyCode keycode);
static GHOST_TKey ghost_key_from_keysym_or_keycode(const KeySym key_sym,
                                                   const XkbDescPtr xkb_descr,
                                                   const KeyCode keycode);

/* these are for copy and select copy */
static char *txt_cut_buffer = nullptr;
static char *txt_select_buffer = nullptr;

#ifdef WITH_XWAYLAND_HACK
static bool use_xwayland_hack = false;
#endif

using namespace std;

GHOST_SystemX11::GHOST_SystemX11()
    : GHOST_System(),
      xkb_descr_(nullptr),
      keyboard_vector_{0},
#ifdef WITH_X11_XINPUT
      last_key_time_(0),
#endif
      keycode_last_repeat_key_(uint(-1))
{
  XInitThreads();
  display_ = XOpenDisplay(nullptr);

  if (!display_) {
    throw std::runtime_error("unable to open a display!");
  }

#ifdef USE_X11_ERROR_HANDLERS
  (void)XSetErrorHandler(GHOST_X11_ApplicationErrorHandler);
  (void)XSetIOErrorHandler(GHOST_X11_ApplicationIOErrorHandler);
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  /* NOTE: Don't open connection to XIM server here, because the locale has to be
   * set before opening the connection but `setlocale()` has not been called yet.
   * the connection will be opened after entering the event loop. */
  xim_ = nullptr;
#endif

#define GHOST_INTERN_ATOM_IF_EXISTS(atom) \
  { \
    atom_.atom = XInternAtom(display_, #atom, True); \
  } \
  (void)0
#define GHOST_INTERN_ATOM(atom) \
  { \
    atom_.atom = XInternAtom(display_, #atom, False); \
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
  atom_.TABLET = XInternAtom(display_, XI_TABLET, False);
#endif

#undef GHOST_INTERN_ATOM_IF_EXISTS
#undef GHOST_INTERN_ATOM

  last_warp_x_ = 0;
  last_warp_y_ = 0;
  last_release_keycode_ = 0;
  last_release_time_ = 0;

  /* Use detectable auto-repeat, mac and windows also do this. */
  int use_xkb;
  int xkb_opcode, xkb_event, xkb_error;
  int xkb_major = XkbMajorVersion, xkb_minor = XkbMinorVersion;

  use_xkb = XkbQueryExtension(
      display_, &xkb_opcode, &xkb_event, &xkb_error, &xkb_major, &xkb_minor);
  if (use_xkb) {
    XkbSetDetectableAutoRepeat(display_, true, nullptr);

    xkb_descr_ = XkbGetMap(display_, 0, XkbUseCoreKbd);
    if (xkb_descr_) {
      XkbGetNames(display_, XkbKeyNamesMask, xkb_descr_);
      XkbGetControls(display_, XkbPerKeyRepeatMask | XkbRepeatKeysMask, xkb_descr_);
    }
  }

#ifdef WITH_XWAYLAND_HACK
  use_xwayland_hack = getenv("WAYLAND_DISPLAY") != nullptr;
#endif

#ifdef WITH_X11_XINPUT
  /* Detect if we have XINPUT (for reuse). */
  {
    memset(&xinput_version_, 0, sizeof(xinput_version_));
    XExtensionVersion *version = XGetExtensionVersion(display_, INAME);
    if (version && (version != (XExtensionVersion *)NoSuchExtension)) {
      if (version->present) {
        xinput_version_ = *version;
      }
      XFree(version);
    }
  }

#  ifdef USE_XINPUT_HOTPLUG
  if (xinput_version_.present) {
    XEventClass class_presence;
    int xi_presence;
    DevicePresence(display_, xi_presence, class_presence);
    XSelectExtensionEvent(
        display_, RootWindow(display_, DefaultScreen(display_)), &class_presence, 1);
    (void)xi_presence;
  }
#  endif /* USE_XINPUT_HOTPLUG */

  refreshXInputDevices();
#endif /* WITH_X11_XINPUT */
}

GHOST_SystemX11::~GHOST_SystemX11()
{
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
  if (xim_) {
    XCloseIM(xim_);
  }
#endif

#ifdef WITH_X11_XINPUT
  /* Close tablet devices. */
  clearXInputDevices();
#endif /* WITH_X11_XINPUT */

  if (xkb_descr_) {
    XkbFreeKeyboard(xkb_descr_, XkbAllComponentsMask, true);
  }

  XCloseDisplay(display_);
}

GHOST_TSuccess GHOST_SystemX11::init()
{
  GHOST_TSuccess success = GHOST_System::init();

  if (success) {
#ifdef WITH_INPUT_NDOF
    ndof_manager_ = new GHOST_NDOFManagerUnix(*this);
#endif
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

uint64_t GHOST_SystemX11::getMilliSeconds() const
{
  timespec ts = {0, 0};
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    GHOST_ASSERT(false, "Could not instantiate monotonic timer!");
  }
  /* Taking care not to overflow the tv.tv_sec * 1000 */
  const uint64_t time = (uint64_t(ts.tv_sec) * 1000) + uint64_t(ts.tv_nsec / 1000000);
  return time;
}

uint64_t GHOST_SystemX11::ms_from_input_time(Time timestamp) const
{
  /* NOTE(@ideasman42): Return a time compatible with `getMilliSeconds()`,
   * this is needed as X11 time-stamps use monotonic time.
   * The X11 implementation *could* use any basis, in practice though we are supporting
   * XORG/LIBINPUT which uses time-stamps based on the monotonic time,
   * Needed to resolve failure to detect double-clicking, see: #40009. */

  /* Accumulate time rollover (as well as store the initial delta from #getMilliSeconds). */
  static uint64_t timestamp_offset = 0;

  /* The last event time (to detect rollover). */
  static uint32_t timestamp_prev = 0;
  /* Causes the X11 time-stamp to be zero based. */
  static uint32_t timestamp_start = 0;

  static bool is_time_init = false;

#if 0
  /* Force rollover after 2 seconds (for testing). */
  {
    const uint32_t timestamp_wrap_ms = 2000;
    static uint32_t timestamp_offset_fake = 0;
    if (!is_time_init) {
      timestamp_offset_fake = UINT32_MAX - (timestamp + timestamp_wrap_ms);
    }
    timestamp = uint32_t(timestamp + timestamp_offset_fake);
  }
#endif

  if (!is_time_init) {
    /* Store the initial delta in the rollover. */
    const uint64_t current_time = getMilliSeconds();
    timestamp_offset = current_time;
    timestamp_start = timestamp;
  }

  /* Always remove the start time.
   * This changes the point where `uint32_t` rolls over, but that's OK. */
  timestamp = uint32_t(timestamp) - timestamp_start;

  if (!is_time_init) {
    is_time_init = true;
    timestamp_prev = timestamp;
  }

  if (UNLIKELY(timestamp < timestamp_prev)) {
    /* Only rollover if this is within a reasonable range. */
    if (UNLIKELY(timestamp_prev - timestamp > UINT32_MAX / 2)) {
      timestamp_offset += uint64_t(UINT32_MAX) + 1;
    }
  }
  timestamp_prev = timestamp;

  uint64_t timestamp_final = (uint64_t(timestamp) + timestamp_offset);

  return timestamp_final;
}

uint8_t GHOST_SystemX11::getNumDisplays() const
{
  return uint8_t(1);
}

void GHOST_SystemX11::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  if (display_) {
    /* NOTE(@ideasman42): for this to work as documented,
     * we would need to use Xinerama check r54370 for code that did this,
     * we've since removed since its not worth the extra dependency. */
    getAllDisplayDimensions(width, height);
  }
}

void GHOST_SystemX11::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  if (display_) {
    width = DisplayWidth(display_, DefaultScreen(display_));
    height = DisplayHeight(display_, DefaultScreen(display_));
  }
}

GHOST_IWindow *GHOST_SystemX11::createWindow(const char *title,
                                             int32_t left,
                                             int32_t top,
                                             uint32_t width,
                                             uint32_t height,
                                             GHOST_TWindowState state,
                                             GHOST_GPUSettings gpu_settings,
                                             const bool exclusive,
                                             const bool is_dialog,
                                             const GHOST_IWindow *parent_window)
{
  GHOST_WindowX11 *window = nullptr;

  if (!display_) {
    return nullptr;
  }

  const GHOST_ContextParams context_params = GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS(gpu_settings);
  window = new GHOST_WindowX11(this,
                               display_,
                               title,
                               left,
                               top,
                               width,
                               height,
                               state,
                               (GHOST_WindowX11 *)parent_window,
                               gpu_settings.context_type,
                               is_dialog,
                               context_params,
                               exclusive,
                               gpu_settings.preferred_device);

  if (window) {
    /* Both are now handle in GHOST_WindowX11.cc
     * Focus and Delete atoms. */

    if (window->getValid()) {
      /* Store the pointer to the window */
      window_manager_->addWindow(window);
      window_manager_->setActiveWindow(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      delete window;
      window = nullptr;
    }
  }
  return window;
}

GHOST_IContext *GHOST_SystemX11::createOffscreenContext(GHOST_GPUSettings gpu_settings)
{
  const GHOST_ContextParams context_params_offscreen =
      GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS_OFFSCREEN(gpu_settings);

  switch (gpu_settings.context_type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(context_params_offscreen,
                                                   GHOST_kVulkanPlatformX11,
                                                   0,
                                                   display_,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   1,
                                                   2,
                                                   gpu_settings.preferred_device);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_OPENGL_BACKEND
    case GHOST_kDrawingContextTypeOpenGL: {
      for (int minor = 6; minor >= 3; --minor) {
        GHOST_Context *context = new GHOST_ContextGLX(
            context_params_offscreen,
            (Window) nullptr,
            display_,
            (GLXFBConfig) nullptr,
            GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            4,
            minor,
            GHOST_OPENGL_GLX_CONTEXT_FLAGS |
                (context_params_offscreen.is_debug ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
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

GHOST_TSuccess GHOST_SystemX11::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
static void destroyIMCallback(XIM /*xim*/, XPointer ptr, XPointer /*data*/)
{
  GHOST_PRINT("XIM server died\n");

  if (ptr) {
    *(XIM *)ptr = nullptr;
  }
}

bool GHOST_SystemX11::openX11_IM()
{
  if (!display_) {
    return false;
  }

  /* set locale modifiers such as `@im=ibus` specified by XMODIFIERS. */
  XSetLocaleModifiers("");

  xim_ = XOpenIM(display_, nullptr, (char *)GHOST_X11_RES_NAME, (char *)GHOST_X11_RES_CLASS);
  if (!xim_) {
    return false;
  }

  XIMCallback destroy;
  destroy.callback = (XIMProc)destroyIMCallback;
  destroy.client_data = (XPointer)&xim_;
  XSetIMValues(xim_, XNDestroyCallback, &destroy, nullptr);
  return true;
}
#endif

GHOST_WindowX11 *GHOST_SystemX11::findGhostWindow(Window xwind) const
{

  if (xwind == 0) {
    return nullptr;
  }

  /* It is not entirely safe to do this as the back-pointer may point
   * to a window that has recently been removed.
   * We should always check the window manager's list of windows
   * and only process events on these windows. */

  const vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();

  vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
  vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();

  for (; win_it != win_end; ++win_it) {
    GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
    if (window->getXWindow() == xwind) {
      return window;
    }
  }
  return nullptr;
}

static void SleepTillEvent(Display *display, int64_t maxSleep)
{
  int fd = ConnectionNumber(display);
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  if (maxSleep == -1) {
    select(fd + 1, &fds, nullptr, nullptr, nullptr);
  }
  else {
    timeval tv;

    tv.tv_sec = maxSleep / 1000;
    tv.tv_usec = (maxSleep - tv.tv_sec * 1000) * 1000;

    select(fd + 1, &fds, nullptr, nullptr, &tv);
  }
}

/* This function borrowed from QT's X11 support `qclipboard_x11.cpp`. */
struct init_timestamp_data {
  Time timestamp;
};

static Bool init_timestamp_scanner(Display * /*display*/, XEvent *event, XPointer arg)
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
  XCheckIfEvent(display_, &ev, &init_timestamp_scanner, (XPointer)&data);

  return data.timestamp;
}

bool GHOST_SystemX11::processEvents(bool waitForEvent)
{
  /* Get all the current events -- translate them into
   * ghost events and call base class pushEvent() method. */

  bool anyProcessed = false;

  do {
    GHOST_TimerManager *timerMgr = getTimerManager();

    if (waitForEvent && dirty_windows_.empty() && !XPending(display_)) {
      uint64_t next = timerMgr->nextFireTime();

      if (next == GHOST_kFireTimeNever) {
        SleepTillEvent(display_, -1);
      }
      else {
        const int64_t maxSleep = next - getMilliSeconds();

        if (maxSleep >= 0) {
          SleepTillEvent(display_, next - getMilliSeconds());
        }
      }
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      anyProcessed = true;
    }

    while (XPending(display_)) {
      XEvent xevent;
      XNextEvent(display_, &xevent);

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* open connection to XIM server and create input context (XIC)
       * when receiving the first FocusIn or KeyPress event after startup,
       * or recover XIM and XIC when the XIM server has been restarted */
      if (ELEM(xevent.type, FocusIn, KeyPress)) {
        if (!xim_ && openX11_IM()) {
          GHOST_PRINT("Connected to XIM server\n");
        }

        if (xim_) {
          GHOST_WindowX11 *window = findGhostWindow(xevent.xany.window);
          if (window && !window->getX11_XIC() && window->createX11_XIC()) {
            GHOST_PRINT("XIM input context created\n");
            if (xevent.type == KeyPress) {
              /* we can assume the window has input focus
               * here, because key events are received only
               * when the window is focused. */
              XSetICFocus(window->getX11_XIC());
            }
          }
        }
      }

      /* Ensure generated time-stamps are non-zero. */
      if (ELEM(xevent.type, KeyPress, KeyRelease)) {
        if (xevent.xkey.time != 0) {
          last_key_time_ = xevent.xkey.time;
        }
      }

      /* dispatch event to XIM server */
      if (XFilterEvent(&xevent, (Window) nullptr) == True) {
        /* do nothing now, the event is consumed by XIM. */
        continue;
      }
#endif
      /* When using auto-repeat, some key-press events can actually come *after* the
       * last key-release. The next code takes care of that. */
      if (xevent.type == KeyRelease) {
        last_release_keycode_ = xevent.xkey.keycode;
        last_release_time_ = xevent.xkey.time;
      }
      else if (xevent.type == KeyPress) {
        if ((xevent.xkey.keycode == last_release_keycode_) &&
            (xevent.xkey.time <= last_release_time_))
        {
          continue;
        }
      }

      processEvent(&xevent);
      anyProcessed = true;

#ifdef USE_UNITY_WORKAROUND
      /* NOTE: processEvent() can't include this code because
       * KeymapNotify event have no valid window information. */

      /* the X server generates KeymapNotify event immediately after
       * every EnterNotify and FocusIn event.  we handle this event
       * to correct modifier states. */
      if (xevent.type == FocusIn) {
        /* use previous event's window, because KeymapNotify event
         * has no window information. */
        GHOST_WindowX11 *window = findGhostWindow(xevent.xany.window);
        if (window && XPending(display_) >= 2) {
          XNextEvent(display_, &xevent);

          if (xevent.type == KeymapNotify) {
            XEvent xev_next;

            /* check if KeyPress or KeyRelease event was generated
             * in order to confirm the window is active. */
            XPeekEvent(display_, &xev_next);

            if (ELEM(xev_next.type, KeyPress, KeyRelease)) {
              const uint64_t event_ms = ms_from_input_time(xev_next.xkey.time);
              /* XK_Hyper_L/R currently unused. */
              const static KeySym modifiers[] = {
                  XK_Shift_L,
                  XK_Shift_R,
                  XK_Control_L,
                  XK_Control_R,
                  XK_Alt_L,
                  XK_Alt_R,
                  XK_Super_L,
                  XK_Super_R,
                  XK_Hyper_L,
                  XK_Hyper_R,
              };

              for (int i = 0; i < int(ARRAY_SIZE(modifiers)); i++) {
                KeyCode kc = XKeysymToKeycode(display_, modifiers[i]);
                if (kc != 0 && ((xevent.xkeymap.key_vector[kc >> 3] >> (kc & 7)) & 1) != 0) {
                  pushEvent(new GHOST_EventKey(event_ms,
                                               GHOST_kEventKeyDown,
                                               window,
                                               ghost_key_from_keysym(modifiers[i]),
                                               false,
                                               nullptr));
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
    if (static_cast<GHOST_NDOFManagerUnix *>(ndof_manager_)->processEvents()) {
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

  if (device == nullptr) {
    return false;
  }

  /* needed since unplugging will abort() without this */
  GHOST_X11_ERROR_HANDLERS_OVERRIDE(handler_store);

  state = XQueryDeviceState(display, device);

  GHOST_X11_ERROR_HANDLERS_RESTORE(handler_store);

  if (state) {
    XInputClass *cls = state->data;
    // printf("%d class%s :\n", state->num_classes, (state->num_classes > 1) ? "es" : "");
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
  GHOST_Event *g_event = nullptr;

  /* Detect auto-repeat. */
  bool is_repeat = false;
  if (ELEM(xe->type, KeyPress, KeyRelease)) {
    XKeyEvent *xke = &(xe->xkey);

    /* Set to true if this key will repeat. */
    bool is_repeat_keycode = false;

    if (xkb_descr_ != nullptr) {
      /* Use XKB support. */
      is_repeat_keycode = (
          /* Should always be true, check just in case. */
          (xke->keycode < (XkbPerKeyBitArraySize << 3)) &&
          bit_is_on(xkb_descr_->ctrls->per_key_repeat, xke->keycode));
    }
    else {
      /* No XKB support (filter by modifier). */
      switch (XLookupKeysym(xke, 0)) {
        case XK_Shift_L:
        case XK_Shift_R:
        case XK_Control_L:
        case XK_Control_R:
        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Super_L:
        case XK_Super_R:
        case XK_Hyper_L:
        case XK_Hyper_R:
        case XK_Caps_Lock:
        case XK_Scroll_Lock:
        case XK_Num_Lock: {
          break;
        }
        default: {
          is_repeat_keycode = true;
        }
      }
    }

    if (is_repeat_keycode) {
      if (xe->type == KeyPress) {
        if (keycode_last_repeat_key_ == xke->keycode) {
          is_repeat = true;
        }
        keycode_last_repeat_key_ = xke->keycode;
      }
      else {
        if (keycode_last_repeat_key_ == xke->keycode) {
          keycode_last_repeat_key_ = uint(-1);
        }
      }
    }
  }
  else if (xe->type == EnterNotify) {
    /* We can't tell how the key state changed, clear it to avoid stuck keys. */
    keycode_last_repeat_key_ = uint(-1);
  }

#ifdef USE_XINPUT_HOTPLUG
  /* Hot-Plug support */
  if (xinput_version_.present) {
    XEventClass class_presence;
    int xi_presence;

    DevicePresence(display_, xi_presence, class_presence);
    (void)class_presence;

    if (xe->type == xi_presence) {
      const XDevicePresenceNotifyEvent *notify_event = (const XDevicePresenceNotifyEvent *)xe;
      if (ELEM(notify_event->devchange, DeviceEnabled, DeviceDisabled, DeviceAdded, DeviceRemoved))
      {
        refreshXInputDevices();

        /* update all window events */
        {
          const vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();
          vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
          vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();

          for (; win_it != win_end; ++win_it) {
            GHOST_WindowX11 *window_xinput = static_cast<GHOST_WindowX11 *>(*win_it);
            window_xinput->refreshXInputDevices();
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
  if (window->GetTabletData().Active != GHOST_kTabletModeNone) {
    bool any_proximity = false;

    for (const GHOST_TabletX11 &xtablet : xtablets_) {
      if (checkTabletProximity(xe->xany.display, xtablet.Device)) {
        any_proximity = true;
      }
    }

    if (!any_proximity) {
      // printf("proximity disable\n");
      window->GetTabletData().Active = GHOST_kTabletModeNone;
    }
  }
#endif /* WITH_X11_XINPUT */
  switch (xe->type) {
    case Expose: {
      const XExposeEvent &xee = xe->xexpose;

      if (xee.count == 0) {
        /* Only generate a single expose event per read of the event queue. */

        /* Event has no timestamp. */
        const uint64_t event_ms = getMilliSeconds();

        g_event = new GHOST_Event(event_ms, GHOST_kEventWindowUpdate, window);
      }
      break;
    }

    case MotionNotify: {
      const XMotionEvent &xme = xe->xmotion;
      const uint64_t event_ms = ms_from_input_time(xme.time);

      bool is_tablet = window->GetTabletData().Active != GHOST_kTabletModeNone;

      if (is_tablet == false && window->getCursorGrabModeIsWarp()) {
        int32_t x_new = xme.x_root;
        int32_t y_new = xme.y_root;
        int32_t x_accum, y_accum;

        /* Warp within bounds. */
        {
          GHOST_Rect bounds;
          int32_t bounds_margin = 0;
          GHOST_TAxisFlag bounds_axis = GHOST_kAxisNone;

          if (window->getCursorGrabMode() == GHOST_kGrabHide) {
            window->getClientBounds(bounds);

            /* TODO(@ideasman42): warp the cursor to `window->getCursorGrabInitPos`,
             * on every motion event, see: D16557 (alternative fix for #102346). */
            const int32_t subregion_div = 4; /* One quarter of the region. */
            const int32_t size[2] = {bounds.getWidth(), bounds.getHeight()};
            const int32_t center[2] = {
                (bounds.l_ + bounds.r_) / 2,
                (bounds.t_ + bounds.b_) / 2,
            };
            /* Shrink the box to prevent the cursor escaping. */
            bounds.l_ = center[0] - (size[0] / (subregion_div * 2));
            bounds.r_ = center[0] + (size[0] / (subregion_div * 2));
            bounds.t_ = center[1] - (size[1] / (subregion_div * 2));
            bounds.b_ = center[1] + (size[1] / (subregion_div * 2));
            bounds_margin = 0;
            bounds_axis = GHOST_TAxisFlag(GHOST_kAxisX | GHOST_kAxisY);
          }
          else {
            /* Fallback to window bounds. */
            if (window->getCursorGrabBounds(bounds) == GHOST_kFailure) {
              window->getClientBounds(bounds);
            }
            /* Could also clamp to screen bounds wrap with a window outside the view will
             * fail at the moment. Use offset of 8 in case the window is at screen bounds. */
            bounds_margin = 8;
            bounds_axis = window->getCursorGrabAxis();
          }

          /* Could also clamp to screen bounds wrap with a window outside the view will
           * fail at the moment. Use inset in case the window is at screen bounds. */
          bounds.wrapPoint(x_new, y_new, bounds_margin, bounds_axis);
        }

        window->getCursorGrabAccum(x_accum, y_accum);

        if (x_new != xme.x_root || y_new != xme.y_root) {
          /* Use time of last event to avoid wrapping several times on the 'same' actual wrap.
           * Note that we need to deal with X and Y separately as those might wrap at the same time
           * but still in two different events (corner case, see #74918).
           * We also have to add a few extra milliseconds of 'padding', as sometimes we get two
           * close events that will generate extra wrap on the same axis within those few
           * milliseconds. */
          if (x_new != xme.x_root && xme.time > last_warp_x_) {
            x_accum += (xme.x_root - x_new);
            last_warp_x_ = lastEventTime(xme.time) + 25;
          }
          if (y_new != xme.y_root && xme.time > last_warp_y_) {
            y_accum += (xme.y_root - y_new);
            last_warp_y_ = lastEventTime(xme.time) + 25;
          }
          window->setCursorGrabAccum(x_accum, y_accum);
          /* When wrapping we don't need to add an event because the
           * #setCursorPosition call will cause a new event after. */
          setCursorPosition(x_new, y_new); /* wrap */
        }
        else {
          g_event = new GHOST_EventCursor(event_ms,
                                          GHOST_kEventCursorMove,
                                          window,
                                          xme.x_root + x_accum,
                                          xme.y_root + y_accum,
                                          window->GetTabletData());
        }
      }
      else {
        g_event = new GHOST_EventCursor(event_ms,
                                        GHOST_kEventCursorMove,
                                        window,
                                        xme.x_root,
                                        xme.y_root,
                                        window->GetTabletData());
      }
      break;
    }

    case KeyPress:
    case KeyRelease: {
      XKeyEvent *xke = &(xe->xkey);
#ifdef WITH_X11_XINPUT
      /* Can be zero for XIM generated events. */
      const Time time = xke->time ? xke->time : last_key_time_;
#else
      const Time time = xke->time;
#endif
      const uint64_t event_ms = ms_from_input_time(time);

      KeySym key_sym;
      char *utf8_buf = nullptr;
      char ascii;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* `utf8_array[]` is initial buffer used for #Xutf8LookupString().
       * if the length of the UTF8 string exceeds this array, allocate
       * another memory area and call #Xutf8LookupString() again.
       * the last 5 bytes are used to avoid segfault that might happen
       * at the end of this buffer when the constructor of #GHOST_EventKey
       * reads 6 bytes regardless of the effective data length. */
      char utf8_array[16 * 6 + 5]; /* 16 UTF8 characters. */
      int len = 1;                 /* At least one null character will be stored. */
#else
      char utf8_array[sizeof(GHOST_TEventKeyData::utf8_buf)] = {'\0'};
#endif
      GHOST_TEventType type = (xke->type == KeyPress) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp;

      GHOST_TKey gkey;

#ifdef USE_NON_LATIN_KB_WORKAROUND
      /* XXX: Code below is kinda awfully convoluted... Issues are:
       * - In keyboards like Latin ones, numbers need a 'Shift' to be accessed but key_sym
       *   is unmodified (or anyone swapping the keys with `xmodmap`).
       * - #XLookupKeysym seems to always use first defined key-map (see #47228), which generates
       *   key-codes unusable by #ghost_key_from_keysym for non-Latin-compatible key-maps.
       *
       * To address this, we:
       * - Try to get a 'number' key_sym using #XLookupKeysym (with virtual shift modifier),
       *   in a very restrictive set of cases.
       * - Fallback to #XLookupString to get a key_sym from active user-defined key-map.
       *
       * Note that:
       * - This effectively 'lock' main number keys to always output number events
       *   (except when using alt-gr).
       * - This enforces users to use an ASCII-compatible key-map with Blender -
       *   but at least it gives predictable and consistent results.
       *
       * Also, note that nothing in XLib sources [1] makes it obvious why those two functions give
       * different key_sym results.
       *
       * [1] http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/KeyBind.c
       */
      KeySym key_sym_str;
      /* Mode_switch 'modifier' is `AltGr` - when this one or Shift are enabled,
       * we do not want to apply that 'forced number' hack. */
      const uint mode_switch_mask = XkbKeysymToModifiers(xke->display, XK_Mode_switch);
      const uint number_hack_forbidden_kmods_mask = mode_switch_mask | ShiftMask;
      if ((xke->keycode >= 10 && xke->keycode < 20) &&
          ((xke->state & number_hack_forbidden_kmods_mask) == 0))
      {
        key_sym = XLookupKeysym(xke, ShiftMask);
        if (!((key_sym >= XK_0) && (key_sym <= XK_9))) {
          key_sym = XLookupKeysym(xke, 0);
        }
      }
      else {
        key_sym = XLookupKeysym(xke, 0);
      }

      if (!XLookupString(xke, &ascii, 1, &key_sym_str, nullptr)) {
        ascii = '\0';
      }

      /* Only allow a limited set of keys from XLookupKeysym,
       * all others we take from XLookupString, unless it gives unknown key... */
      gkey = ghost_key_from_keysym_or_keycode(key_sym, xkb_descr_, xke->keycode);
      switch (gkey) {
        case GHOST_kKeyRightAlt:
        case GHOST_kKeyLeftAlt:
        case GHOST_kKeyRightShift:
        case GHOST_kKeyLeftShift:
        case GHOST_kKeyRightControl:
        case GHOST_kKeyLeftControl:
        case GHOST_kKeyLeftOS:
        case GHOST_kKeyRightOS:
        case GHOST_kKeyLeftHyper:
        case GHOST_kKeyRightHyper:
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
      /* In keyboards like Latin ones,
       * numbers needs a 'Shift' to be accessed but key_sym
       * is unmodified (or anyone swapping the keys with `xmodmap`).
       *
       * Here we look at the 'Shifted' version of the key.
       * If it is a number, then we take it instead of the normal key.
       *
       * The modified key is sent in the `ascii`s variable anyway.
       */
      if ((xke->keycode >= 10 && xke->keycode < 20) &&
          ((key_sym = XLookupKeysym(xke, ShiftMask)) >= XK_0) && (key_sym <= XK_9))
      {
        /* Pass (keep shifted `key_sym`). */
      }
      else {
        /* regular case */
        key_sym = XLookupKeysym(xke, 0);
      }

      gkey = ghost_key_from_keysym_or_keycode(key_sym, xkb_descr_, xke->keycode);

      if (!XLookupString(xke, &ascii, 1, nullptr, nullptr)) {
        ascii = '\0';
      }
#endif

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* Only used for key-press. */
      XIC xic = nullptr;
#endif

      if (xke->type == KeyPress) {
        utf8_buf = utf8_array;
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
        /* Setting unicode on key-up events gives #XLookupNone status. */
        xic = window->getX11_XIC();
        if (xic) {
          Status status;

          /* Use UTF8 because its not locale repentant, from XORG docs. */
          if (!(len = Xutf8LookupString(
                    xic, xke, utf8_buf, sizeof(utf8_array) - 5, &key_sym, &status)))
          {
            utf8_buf[0] = '\0';
          }

          if (status == XBufferOverflow) {
            utf8_buf = (char *)malloc(len + 5);
            len = Xutf8LookupString(xic, xke, utf8_buf, len, &key_sym, &status);
          }

          if (ELEM(status, XLookupChars, XLookupBoth)) {
            /* Check for ASCII control characters.
             * Inline `iscntrl` because the users locale must not change behavior. */
            if ((utf8_buf[0] < 32 && utf8_buf[0] > 0) || (utf8_buf[0] == 127)) {
              utf8_buf[0] = '\0';
            }
          }
          else if (status == XLookupKeySym) {
            /* this key doesn't have a text representation, it is a command
             * key of some sort */
          }
          else {
            printf("Bad keycode lookup. Keysym 0x%x Status: %s\n",
                   uint(key_sym),
                   (status == XLookupNone   ? "XLookupNone" :
                    status == XLookupKeySym ? "XLookupKeySym" :
                                              "Unknown status"));

            printf("'%.*s' %p %p\n", len, utf8_buf, xic, xim_);
          }
        }
        else {
          utf8_buf[0] = '\0';
        }
#endif
        if (!utf8_buf[0] && ascii) {
          utf8_buf[0] = ascii;
          utf8_buf[1] = '\0';
        }
      }

      g_event = new GHOST_EventKey(event_ms, type, window, gkey, is_repeat, utf8_buf);

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      /* When using IM for some languages such as Japanese,
       * one event inserts multiple UTF8 characters. */
      if (xke->type == KeyPress && xic) {
        uchar c;
        int i = 0;
        while (true) {
          /* Search character boundary. */
          if (uchar(utf8_buf[i++]) > 0x7f) {
            for (; i < len; ++i) {
              c = utf8_buf[i];
              if (c < 0x80 || c > 0xbf) {
                break;
              }
            }
          }

          if (i >= len) {
            break;
          }
          /* Enqueue previous character. */
          pushEvent(g_event);

          g_event = new GHOST_EventKey(event_ms, type, window, gkey, is_repeat, &utf8_buf[i]);
        }
      }

      if (utf8_buf != utf8_array) {
        free(utf8_buf);
      }
#endif

      break;
    }

    case ButtonPress:
    case ButtonRelease: {
      const XButtonEvent &xbe = xe->xbutton;
      const uint64_t event_ms = ms_from_input_time(xbe.time);
      GHOST_TButton gbmask = GHOST_kButtonMaskLeft;
      GHOST_TEventType type = (xbe.type == ButtonPress) ? GHOST_kEventButtonDown :
                                                          GHOST_kEventButtonUp;

      /* process wheel mouse events and break, only pass on press events */
      if (xbe.button == Button4) {
        if (xbe.type == ButtonPress) {
          g_event = new GHOST_EventWheel(event_ms, window, GHOST_kEventWheelAxisVertical, 1);
        }
        break;
      }
      if (xbe.button == Button5) {
        if (xbe.type == ButtonPress) {
          g_event = new GHOST_EventWheel(event_ms, window, GHOST_kEventWheelAxisVertical, -1);
        }
        break;
      }

      /* process rest of normal mouse buttons */
      if (xbe.button == Button1) {
        gbmask = GHOST_kButtonMaskLeft;
      }
      else if (xbe.button == Button2) {
        gbmask = GHOST_kButtonMaskMiddle;
      }
      else if (xbe.button == Button3) {
        gbmask = GHOST_kButtonMaskRight;
        /* It seems events 6 and 7 are for horizontal scrolling.
         * you can re-order button mapping like this... (swaps 6,7 with 8,9)
         * `xmodmap -e "pointer = 1 2 3 4 5 8 9 6 7"` */
      }
      else if (xbe.button == 6) {
        gbmask = GHOST_kButtonMaskButton6;
      }
      else if (xbe.button == 7) {
        gbmask = GHOST_kButtonMaskButton7;
      }
      else if (xbe.button == 8) {
        gbmask = GHOST_kButtonMaskButton4;
      }
      else if (xbe.button == 9) {
        gbmask = GHOST_kButtonMaskButton5;
      }
      else {
        break;
      }

      g_event = new GHOST_EventButton(event_ms, type, window, gbmask, window->GetTabletData());
      break;
    }

    /* change of size, border, layer etc. */
    case ConfigureNotify: {
      // const XConfigureEvent & xce = xe->xconfigure;
      /* Event has no timestamp. */
      const uint64_t event_ms = getMilliSeconds();

      g_event = new GHOST_Event(event_ms, GHOST_kEventWindowSize, window);
      break;
    }

    case FocusIn:
    case FocusOut: {
      const XFocusChangeEvent &xfe = xe->xfocus;

      /* TODO: make sure this is the correct place for activate/deactivate */
#if 0
      printf("X: focus %s for window %d\n", xfe.type == FocusIn ? "in" : "out", int(xfe.window));
#endif

      /* May have to look at the type of event and filter some out. */

      GHOST_TEventType gtype = (xfe.type == FocusIn) ? GHOST_kEventWindowActivate :
                                                       GHOST_kEventWindowDeactivate;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
      XIC xic = window->getX11_XIC();
      if (xic) {
        if (xe->type == FocusIn) {
          XSetICFocus(xic);
        }
        else {
          XUnsetICFocus(xic);
        }
      }
#endif

      g_event = new GHOST_Event(getMilliSeconds(), gtype, window);
      break;
    }
    case ClientMessage: {
      XClientMessageEvent &xcme = xe->xclient;

      if (((Atom)xcme.data.l[0]) == atom_.WM_DELETE_WINDOW) {
        g_event = new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window);
      }
      else if (((Atom)xcme.data.l[0]) == atom_.WM_TAKE_FOCUS) {
        XWindowAttributes attr;
        Window fwin;
        int revert_to;

        /* as ICCCM say, we need reply this event
         * with a #SetInputFocus, the data[1] have
         * the valid timestamp (send by the wm).
         *
         * Some WM send this event before the
         * window is really mapped (for example
         * change from virtual desktop), so we need
         * to be sure that our windows is mapped
         * or this call fail and close blender.
         */
        if (XGetWindowAttributes(display_, xcme.window, &attr) == True) {
          if (XGetInputFocus(display_, &fwin, &revert_to) == True) {
            if (attr.map_state == IsViewable) {
              if (fwin != xcme.window) {
                XSetInputFocus(display_, xcme.window, RevertToParent, xcme.data.l[1]);
              }
            }
          }
        }
      }
      else {
#ifdef WITH_XDND
        /* try to handle drag event
         * (if there's no such events, #GHOST_HandleClientMessage will return zero) */
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
      /* #XCrossingEvents pointer leave enter window.
       * also do cursor move here, #MotionNotify only
       * happens when motion starts & ends inside window.
       * we only do moves when the crossing mode is 'normal'
       * (really crossing between windows) since some window-managers
       * also send grab/un-grab crossings for mouse-wheel events.
       */
      const XCrossingEvent &xce = xe->xcrossing;
      const uint64_t event_ms = ms_from_input_time(xce.time);
      if (xce.mode == NotifyNormal) {
        g_event = new GHOST_EventCursor(event_ms,
                                        GHOST_kEventCursorMove,
                                        window,
                                        xce.x_root,
                                        xce.y_root,
                                        window->GetTabletData());
      }

#if 0
      printf(
          "X: %s window %d\n", xce.type == EnterNotify ? "entering" : "leaving", int(xce.window));
#endif

      if (xce.type == EnterNotify) {
        window_manager_->setActiveWindow(window);
      }
      else {
        window_manager_->setWindowInactive(window);
      }

      break;
    }
    case MapNotify:
      /*
       * From ICCCM:
       * [ Clients can select for #StructureNotify on their
       *   top-level windows to track transition between
       *   Normal and Iconic states. Receipt of a #MapNotify
       *   event will indicate a transition to the Normal
       *   state, and receipt of an #UnmapNotify event will
       *   indicate a transition to the Iconic state. ]
       */
      if (window->post_init_ == True) {
        /*
         * Now we are sure that the window is
         * mapped, so only need change the state.
         */
        window->setState(window->post_state_);
        window->post_init_ = False;
      }
      break;
    case UnmapNotify:
      break;
    case MappingNotify:
    case ReparentNotify:
      break;
    case SelectionRequest: {
      XEvent nxe;
      XSelectionRequestEvent *xse = &xe->xselectionrequest;

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

      /* Check to see if the requester is asking for String */
      if (ELEM(xse->target, atom_.UTF8_STRING, atom_.STRING, atom_.COMPOUND_TEXT, atom_.C_STRING))
      {
        if (xse->selection == XInternAtom(display_, "PRIMARY", False)) {
          XChangeProperty(display_,
                          xse->requestor,
                          xse->property,
                          xse->target,
                          8,
                          PropModeReplace,
                          (uchar *)txt_select_buffer,
                          strlen(txt_select_buffer));
        }
        else if (xse->selection == XInternAtom(display_, "CLIPBOARD", False)) {
          XChangeProperty(display_,
                          xse->requestor,
                          xse->property,
                          xse->target,
                          8,
                          PropModeReplace,
                          (uchar *)txt_cut_buffer,
                          strlen(txt_cut_buffer));
        }
      }
      else if (xse->target == atom_.TARGETS) {
        const Atom atom_list[] = {
            atom_.TARGETS, atom_.UTF8_STRING, atom_.STRING, atom_.COMPOUND_TEXT, atom_.C_STRING};
        XChangeProperty(display_,
                        xse->requestor,
                        xse->property,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        reinterpret_cast<const uchar *>(atom_list),
                        ARRAY_SIZE(atom_list));
        XFlush(display_);
      }
      else {
        /* Change property to None because we do not support the selection request target. */
        nxe.xselection.property = None;
      }

      /* Send the event to the client 0 0 == False, #SelectionNotify */
      XSendEvent(display_, xse->requestor, 0, 0, &nxe);
      XFlush(display_);
      break;
    }

    default: {
#ifdef WITH_X11_XINPUT
      for (GHOST_TabletX11 &xtablet : xtablets_) {
        if (ELEM(xe->type, xtablet.MotionEvent, xtablet.PressEvent)) {
          const XDeviceMotionEvent *data = (const XDeviceMotionEvent *)xe;
          if (data->deviceid != xtablet.ID) {
            continue;
          }

          const uchar axis_first = data->first_axis;
          const uchar axes_end = axis_first + data->axes_count; /* after the last */
          int axis_value;

          /* stroke might begin without leading ProxyIn event,
           * this happens when window is opened when stylus is already hovering
           * around tablet surface */
          window->GetTabletData().Active = xtablet.mode;

          /* NOTE: This event might be generated with incomplete data-set
           * (don't exactly know why, looks like in some cases, if the value does not change,
           * it is not included in subsequent #XDeviceMotionEvent events).
           * So we have to check which values this event actually contains!
           */

#  define AXIS_VALUE_GET(axis, val) \
    ((axis_first <= axis && axes_end > axis) && \
     ((void)(val = data->axis_data[axis - axis_first]), true))

          if (AXIS_VALUE_GET(2, axis_value)) {
            window->GetTabletData().Pressure = axis_value / float(xtablet.PressureLevels);
          }

          /* NOTE(@broken): the `short` cast and the & 0xffff is bizarre and unexplained anywhere,
           * but I got garbage data without it. Found it in the `xidump.c` source.
           *
           * NOTE(@mont29): The '& 0xffff' just truncates the value to its two lowest bytes,
           * this probably means some drivers do not properly set the whole int value?
           * Since we convert to float afterward,
           * I don't think we need to cast to short here, but do not have a device to check this.
           */
          if (AXIS_VALUE_GET(3, axis_value)) {
            window->GetTabletData().Xtilt = short(axis_value & 0xffff) /
                                            float(xtablet.XtiltLevels);
          }
          if (AXIS_VALUE_GET(4, axis_value)) {
            window->GetTabletData().Ytilt = short(axis_value & 0xffff) /
                                            float(xtablet.YtiltLevels);
          }

#  undef AXIS_VALUE_GET
        }
        else if (xe->type == xtablet.ProxInEvent) {
          const XProximityNotifyEvent *data = (const XProximityNotifyEvent *)xe;
          if (data->deviceid != xtablet.ID) {
            continue;
          }

          window->GetTabletData().Active = xtablet.mode;
        }
        else if (xe->type == xtablet.ProxOutEvent) {
          window->GetTabletData().Active = GHOST_kTabletModeNone;
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

GHOST_TSuccess GHOST_SystemX11::getPixelAtCursor(float r_color[3]) const
{
  /* NOTE: There are known issues/limitations at the moment:
   *
   * - Blender has no control of the cursor outside of its window, so it is
   *   not going to be the eyedropper icon.
   * - GHOST does not report click events from outside of the window, so the
   *   user needs to press Enter instead.
   *
   * Ref #111303. */

  XColor c;
  int32_t x, y;

  if (getCursorPosition(x, y) == GHOST_kFailure) {
    return GHOST_kFailure;
  }
  XImage *image = XGetImage(
      display_, XRootWindow(display_, XDefaultScreen(display_)), x, y, 1, 1, AllPlanes, XYPixmap);
  if (image == nullptr) {
    return GHOST_kFailure;
  }
  c.pixel = XGetPixel(image, 0, 0);
  XFree(image);
  XQueryColor(display_, XDefaultColormap(display_, XDefaultScreen(display_)), &c);

  /* X11 returns colors in the [0, 65535] range, so we need to scale back to [0, 1]. */
  r_color[0] = c.red / 65535.0f;
  r_color[1] = c.green / 65535.0f;
  r_color[2] = c.blue / 65535.0f;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemX11::getModifierKeys(GHOST_ModifierKeys &keys) const
{

  /* Analyze the masks returned from #XQueryPointer. */

  memset((void *)keyboard_vector_, 0, sizeof(keyboard_vector_));

  XQueryKeymap(display_, (char *)keyboard_vector_);

  /* Now translate key symbols into key-codes and test with vector. */

  const static KeyCode shift_l = XKeysymToKeycode(display_, XK_Shift_L);
  const static KeyCode shift_r = XKeysymToKeycode(display_, XK_Shift_R);
  const static KeyCode control_l = XKeysymToKeycode(display_, XK_Control_L);
  const static KeyCode control_r = XKeysymToKeycode(display_, XK_Control_R);
  const static KeyCode alt_l = XKeysymToKeycode(display_, XK_Alt_L);
  const static KeyCode alt_r = XKeysymToKeycode(display_, XK_Alt_R);
  const static KeyCode super_l = XKeysymToKeycode(display_, XK_Super_L);
  const static KeyCode super_r = XKeysymToKeycode(display_, XK_Super_R);
  const static KeyCode hyper_l = XKeysymToKeycode(display_, XK_Hyper_L);
  const static KeyCode hyper_r = XKeysymToKeycode(display_, XK_Hyper_R);

  /* shift */
  keys.set(GHOST_kModifierKeyLeftShift,
           ((keyboard_vector_[shift_l >> 3] >> (shift_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightShift,
           ((keyboard_vector_[shift_r >> 3] >> (shift_r & 7)) & 1) != 0);
  /* control */
  keys.set(GHOST_kModifierKeyLeftControl,
           ((keyboard_vector_[control_l >> 3] >> (control_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightControl,
           ((keyboard_vector_[control_r >> 3] >> (control_r & 7)) & 1) != 0);
  /* alt */
  keys.set(GHOST_kModifierKeyLeftAlt, ((keyboard_vector_[alt_l >> 3] >> (alt_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightAlt, ((keyboard_vector_[alt_r >> 3] >> (alt_r & 7)) & 1) != 0);
  /* super (windows) - only one GHOST-kModifierKeyOS, so mapping to either */
  keys.set(GHOST_kModifierKeyLeftOS, ((keyboard_vector_[super_l >> 3] >> (super_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightOS,
           ((keyboard_vector_[super_r >> 3] >> (super_r & 7)) & 1) != 0);
  /* hyper */
  keys.set(GHOST_kModifierKeyLeftHyper,
           ((keyboard_vector_[hyper_l >> 3] >> (hyper_l & 7)) & 1) != 0);
  keys.set(GHOST_kModifierKeyRightHyper,
           ((keyboard_vector_[hyper_r >> 3] >> (hyper_r & 7)) & 1) != 0);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemX11::getButtons(GHOST_Buttons &buttons) const
{
  Window root_return, child_return;
  int rx, ry, wx, wy;
  uint mask_return;

  if (XQueryPointer(display_,
                    RootWindow(display_, DefaultScreen(display_)),
                    &root_return,
                    &child_return,
                    &rx,
                    &ry,
                    &wx,
                    &wy,
                    &mask_return) == True)
  {
    buttons.set(GHOST_kButtonMaskLeft, (mask_return & Button1Mask) != 0);
    buttons.set(GHOST_kButtonMaskMiddle, (mask_return & Button2Mask) != 0);
    buttons.set(GHOST_kButtonMaskRight, (mask_return & Button3Mask) != 0);
    buttons.set(GHOST_kButtonMaskButton4, (mask_return & Button4Mask) != 0);
    buttons.set(GHOST_kButtonMaskButton5, (mask_return & Button5Mask) != 0);
  }
  else {
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

static GHOST_TSuccess getCursorPosition_impl(Display *display,
                                             int32_t &x,
                                             int32_t &y,
                                             Window *child_return)
{
  int rx, ry, wx, wy;
  uint mask_return;
  Window root_return;

  if (XQueryPointer(display,
                    RootWindow(display, DefaultScreen(display)),
                    &root_return,
                    child_return,
                    &rx,
                    &ry,
                    &wx,
                    &wy,
                    &mask_return) == False)
  {
    return GHOST_kFailure;
  }

  x = rx;
  y = ry;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemX11::getCursorPosition(int32_t &x, int32_t &y) const
{
  Window child_return;
  return getCursorPosition_impl(display_, x, y, &child_return);
}

GHOST_TSuccess GHOST_SystemX11::setCursorPosition(int32_t x, int32_t y)
{

  /* This is a brute force move in screen coordinates
   * #XWarpPointer does relative moves so first determine the
   * current pointer position. */

  int cx, cy;

#ifdef WITH_XWAYLAND_HACK
  Window child_return = None;
  if (getCursorPosition_impl(display_, cx, cy, &child_return) == GHOST_kFailure) {
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
      XFixesHideCursor(display_, child_return);
    }
  }
#endif

#if defined(WITH_X11_XINPUT) && defined(USE_X11_XINPUT_WARP)
  if ((xinput_version_.present) && (xinput_version_.major_version >= 2)) {
    /* Needed to account for XInput "Coordinate Transformation Matrix", see #48901 */
    int device_id;
    if (XIGetClientPointer(display_, None, &device_id) != False) {
      XIWarpPointer(display_, device_id, None, None, 0, 0, 0, 0, relx, rely);
    }
  }
  else
#endif
  {
    XWarpPointer(display_, None, None, 0, 0, 0, 0, relx, rely);
  }

#ifdef WITH_XWAYLAND_HACK
  if (use_xwayland_hack) {
    if (child_return != None) {
      XFixesShowCursor(display_, child_return);
    }
  }
#endif

  XSync(display_, 0); /* Sync to process all requests */

  return GHOST_kSuccess;
}

GHOST_TCapabilityFlag GHOST_SystemX11::getCapabilities() const
{
  return GHOST_TCapabilityFlag(
      GHOST_CAPABILITY_FLAG_ALL &
      /* NOTE: order the following flags as they they're declared in the source. */
      ~(
          /* No support yet for image copy/paste. */
          GHOST_kCapabilityClipboardImage |
          /* No support yet for IME input methods. */
          GHOST_kCapabilityInputIME |
          /* No support for window decoration styles. */
          GHOST_kCapabilityWindowDecorationStyles |
          /* No support yet for RGBA mouse cursors. */
          GHOST_kCapabilityCursorRGBA |
          /* No support yet for dynamic cursor generation. */
          GHOST_kCapabilityCursorGenerator |
          /* No support for window path meta-data. */
          GHOST_kCapabilityWindowPath));
}

void GHOST_SystemX11::addDirtyWindow(GHOST_WindowX11 *bad_wind)
{
  GHOST_ASSERT((bad_wind != nullptr), "addDirtyWindow() nullptr ptr trapped (window)");

  dirty_windows_.push_back(bad_wind);
}

bool GHOST_SystemX11::generateWindowExposeEvents()
{
  vector<GHOST_WindowX11 *>::const_iterator w_start = dirty_windows_.begin();
  vector<GHOST_WindowX11 *>::const_iterator w_end = dirty_windows_.end();
  bool anyProcessed = false;

  for (; w_start != w_end; ++w_start) {
    const GHOST_Event *g_event = new GHOST_Event(
        getMilliSeconds(), GHOST_kEventWindowUpdate, *w_start);

    (*w_start)->validate();

    if (g_event) {
      pushEvent(g_event);
      anyProcessed = true;
    }
  }

  dirty_windows_.clear();
  return anyProcessed;
}

static GHOST_TKey ghost_key_from_keysym_or_keycode(const KeySym key_sym,
                                                   XkbDescPtr xkb_descr,
                                                   const KeyCode keycode)
{
  GHOST_TKey type = ghost_key_from_keysym(key_sym);
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
      GXMAP(type, XK_Super_L, GHOST_kKeyLeftOS);
      GXMAP(type, XK_Super_R, GHOST_kKeyRightOS);
      GXMAP(type, XK_Hyper_L, GHOST_kKeyLeftHyper);
      GXMAP(type, XK_Hyper_R, GHOST_kKeyRightHyper);

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
      GXMAP(type, XK_Menu, GHOST_kKeyApp);

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

      /* Media keys in some keyboards and laptops with XFree86/XORG. */
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
#ifdef WITH_GHOST_DEBUG
        printf("%s: unknown key: %lu / 0x%lx\n", __func__, key, key);
#endif
        type = GHOST_kKeyUnknown;
        break;
    }
  }

  return type;
}

#undef GXMAP

#define MAKE_ID(a, b, c, d) (int(d) << 24 | int(c) << 16 | (b) << 8 | (a))

static GHOST_TKey ghost_key_from_keycode(const XkbDescPtr xkb_descr, const KeyCode keycode)
{
  GHOST_ASSERT(XkbKeyNameLength == 4, "Name length is invalid!");
  if (keycode >= xkb_descr->min_key_code && keycode <= xkb_descr->max_key_code) {
    const char *id_str = xkb_descr->names->keys[keycode].name;
    const uint32_t id = MAKE_ID(id_str[0], id_str[1], id_str[2], id_str[3]);
    switch (id) {
      case MAKE_ID('T', 'L', 'D', 'E'):
        return GHOST_kKeyAccentGrave;
      case MAKE_ID('L', 'S', 'G', 'T'):
        return GHOST_kKeyGrLess;
#ifdef WITH_GHOST_DEBUG
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

/* From `xclip.c` #xcout() v0.11. */

#define XCLIB_XCOUT_NONE 0          /* no context */
#define XCLIB_XCOUT_SENTCONVSEL 1   /* sent a request */
#define XCLIB_XCOUT_INCR 2          /* in an incr loop */
#define XCLIB_XCOUT_FALLBACK 3      /* STRING failed, need fallback to UTF8 */
#define XCLIB_XCOUT_FALLBACK_UTF8 4 /* UTF8 failed, move to compound. */
#define XCLIB_XCOUT_FALLBACK_COMP 5 /* compound failed, move to text. */
#define XCLIB_XCOUT_FALLBACK_TEXT 6

/* Retrieves the contents of a selections. */
void GHOST_SystemX11::getClipboard_xcout(
    const XEvent *evt, Atom sel, Atom target, uchar **txt, ulong *len, uint *context) const
{
  Atom pty_type;
  int pty_format;
  uchar *buffer;
  ulong pty_size, pty_items;
  uchar *ltxt = *txt;

  const vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();
  vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  Window win = window->getXWindow();

  switch (*context) {
    /* There is no context, do an XConvertSelection() */
    case XCLIB_XCOUT_NONE:
      /* Initialize return length to 0. */
      if (*len > 0) {
        free(*txt);
        *len = 0;
      }

      /* Send a selection request */
      XConvertSelection(display_, sel, target, atom_.XCLIP_OUT, win, CurrentTime);
      *context = XCLIB_XCOUT_SENTCONVSEL;
      return;

    case XCLIB_XCOUT_SENTCONVSEL:
      if (evt->type != SelectionNotify) {
        return;
      }

      if (target == atom_.UTF8_STRING && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_UTF8;
        return;
      }
      if (target == atom_.COMPOUND_TEXT && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_COMP;
        return;
      }
      if (target == atom_.TEXT && evt->xselection.property == None) {
        *context = XCLIB_XCOUT_FALLBACK_TEXT;
        return;
      }

      /* find the size and format of the data in property */
      XGetWindowProperty(display_,
                         win,
                         atom_.XCLIP_OUT,
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

      if (pty_type == atom_.INCR) {
        /* start INCR mechanism by deleting property */
        XDeleteProperty(display_, win, atom_.XCLIP_OUT);
        XFlush(display_);
        *context = XCLIB_XCOUT_INCR;
        return;
      }

      /* If it's not INCR, and not `format == 8`, then there's
       * nothing in the selection (that `xclip` understands, anyway). */

      if (pty_format != 8) {
        *context = XCLIB_XCOUT_NONE;
        return;
      }

      /* Not using INCR mechanism, just read the property. */
      XGetWindowProperty(display_,
                         win,
                         atom_.XCLIP_OUT,
                         0,
                         long(pty_size),
                         False,
                         AnyPropertyType,
                         &pty_type,
                         &pty_format,
                         &pty_items,
                         &pty_size,
                         &buffer);

      /* finished with property, delete it */
      XDeleteProperty(display_, win, atom_.XCLIP_OUT);

      /* copy the buffer to the pointer for returned data */
      ltxt = (uchar *)malloc(pty_items);
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
      if (evt->type != PropertyNotify) {
        return;
      }

      /* skip unless the property has a new value */
      if (evt->xproperty.state != PropertyNewValue) {
        return;
      }

      /* check size and format of the property */
      XGetWindowProperty(display_,
                         win,
                         atom_.XCLIP_OUT,
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
        XDeleteProperty(display_, win, atom_.XCLIP_OUT);
        return;
      }

      if (pty_size == 0) {
        /* no more data, exit from loop */
        XFree(buffer);
        XDeleteProperty(display_, win, atom_.XCLIP_OUT);
        *context = XCLIB_XCOUT_NONE;

        /* this means that an INCR transfer is now
         * complete, return 1 */
        return;
      }

      XFree(buffer);

      /* if we have come this far, the property contains
       * text, we know the size. */
      XGetWindowProperty(display_,
                         win,
                         atom_.XCLIP_OUT,
                         0,
                         long(pty_size),
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
        ltxt = (uchar *)malloc(*len);
      }
      else {
        *len += pty_items;
        ltxt = (uchar *)realloc(ltxt, *len);
      }

      /* add data to ltxt */
      memcpy(&ltxt[*len - pty_items], buffer, pty_items);

      *txt = ltxt;
      XFree(buffer);

      /* delete property to get the next item */
      XDeleteProperty(display_, win, atom_.XCLIP_OUT);
      XFlush(display_);
      return;
  }
}

char *GHOST_SystemX11::getClipboard(bool selection) const
{
  Atom sseln;
  Atom target = atom_.UTF8_STRING;
  Window owner;

  /* From `xclip.c` `doOut()` v0.11. */
  char *sel_buf;
  ulong sel_len = 0;
  XEvent evt;
  uint context = XCLIB_XCOUT_NONE;

  if (selection == True) {
    sseln = atom_.PRIMARY;
  }
  else {
    sseln = atom_.CLIPBOARD;
  }

  const vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();
  vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  Window win = window->getXWindow();

  /* check if we are the owner. */
  owner = XGetSelectionOwner(display_, sseln);
  if (owner == win) {
    if (sseln == atom_.CLIPBOARD) {
      size_t sel_buf_size = strlen(txt_cut_buffer) + 1;
      sel_buf = (char *)malloc(sel_buf_size);
      memcpy(sel_buf, txt_cut_buffer, sel_buf_size);
      return sel_buf;
    }
    size_t sel_buf_size = strlen(txt_select_buffer) + 1;
    sel_buf = (char *)malloc(sel_buf_size);
    memcpy(sel_buf, txt_select_buffer, sel_buf_size);
    return sel_buf;
  }
  if (owner == None) {
    return nullptr;
  }

  /* Restore events so copy doesn't swallow other event types (keyboard/mouse). */
  vector<XEvent> restore_events;

  while (true) {
    /* only get an event if xcout() is doing something */
    bool restore_this_event = false;
    if (context != XCLIB_XCOUT_NONE) {
      XNextEvent(display_, &evt);
      restore_this_event = (evt.type != SelectionNotify);
    }

    /* fetch the selection, or part of it */
    getClipboard_xcout(&evt, sseln, target, (uchar **)&sel_buf, &sel_len, &context);

    if (restore_this_event) {
      restore_events.push_back(evt);
    }

    /* Fallback is needed. Set #XA_STRING to target and restart the loop. */
    if (context == XCLIB_XCOUT_FALLBACK) {
      context = XCLIB_XCOUT_NONE;
      target = atom_.STRING;
      continue;
    }
    if (context == XCLIB_XCOUT_FALLBACK_UTF8) {
      /* utf8 fail, move to compound text. */
      context = XCLIB_XCOUT_NONE;
      target = atom_.COMPOUND_TEXT;
      continue;
    }
    if (context == XCLIB_XCOUT_FALLBACK_COMP) {
      /* Compound text fail, move to text. */
      context = XCLIB_XCOUT_NONE;
      target = atom_.TEXT;
      continue;
    }
    if (context == XCLIB_XCOUT_FALLBACK_TEXT) {
      /* Text fail, nothing else to try, break. */
      context = XCLIB_XCOUT_NONE;
    }

    /* Only continue if #xcout() is doing something. */
    if (context == XCLIB_XCOUT_NONE) {
      break;
    }
  }

  while (!restore_events.empty()) {
    XPutBackEvent(display_, &restore_events.back());
    restore_events.pop_back();
  }

  if (sel_len) {
    /* Only print the buffer out, and free it, if it's not empty. */
    char *tmp_data = (char *)malloc(sel_len + 1);
    memcpy(tmp_data, (char *)sel_buf, sel_len);
    tmp_data[sel_len] = '\0';

    if (sseln == atom_.STRING) {
      XFree(sel_buf);
    }
    else {
      free(sel_buf);
    }

    return tmp_data;
  }
  return nullptr;
}

void GHOST_SystemX11::putClipboard(const char *buffer, bool selection) const
{
  Window window_, owner;

  const vector<GHOST_IWindow *> &win_vec = window_manager_->getWindows();
  vector<GHOST_IWindow *>::const_iterator win_it = win_vec.begin();
  GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
  window_ = window->getXWindow();

  if (buffer) {
    if (selection == False) {
      XSetSelectionOwner(display_, atom_.CLIPBOARD, window_, CurrentTime);
      owner = XGetSelectionOwner(display_, atom_.CLIPBOARD);
      if (txt_cut_buffer) {
        free((void *)txt_cut_buffer);
      }

      size_t buffer_size = strlen(buffer) + 1;
      txt_cut_buffer = (char *)malloc(buffer_size);
      memcpy(txt_cut_buffer, buffer, buffer_size);
    }
    else {
      XSetSelectionOwner(display_, atom_.PRIMARY, window_, CurrentTime);
      owner = XGetSelectionOwner(display_, atom_.PRIMARY);
      if (txt_select_buffer) {
        free((void *)txt_select_buffer);
      }

      size_t buffer_size = strlen(buffer) + 1;
      txt_select_buffer = (char *)malloc(buffer_size);
      memcpy(txt_select_buffer, buffer, buffer_size);
    }

    if (owner != window_) {
      fprintf(stderr, "failed to own primary\n");
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Message Box
 * \{ */

class DialogData {
 public:
  /* Width of the dialog. */
  uint width;
  /* Height of the dialog. */
  uint height;
  /* Default padding (x direction) between controls and edge of dialog. */
  uint padding_x;
  /* Default padding (y direction) between controls and edge of dialog. */
  uint padding_y;
  /* Width of a single button. */
  uint button_width;
  /* Height of a single button. */
  uint button_height;
  /* Inset of a button to its text. */
  uint button_inset_x;
  /* Size of the border of the button. */
  uint button_border_size;
  /* Height of a line of text */
  uint line_height;
  /* Offset of the text inside the button. */
  uint button_text_offset_y;

  /* Construct a new #DialogData with the default settings. */
  DialogData()
      : width(640),
        height(175),
        padding_x(10),
        padding_y(5),
        button_width(130),
        button_height(24),
        button_inset_x(10),
        button_border_size(1),
        line_height(16)
  {
    button_text_offset_y = button_height - line_height;
  }

  void drawButton(Display *display,
                  Window &window,
                  GC &borderGC,
                  GC &buttonGC,
                  uint button_num,
                  const char *label)
  {
    XFillRectangle(display,
                   window,
                   borderGC,
                   width - (padding_x + button_width) * button_num,
                   height - padding_y - button_height,
                   button_width,
                   button_height);

    XFillRectangle(display,
                   window,
                   buttonGC,
                   width - (padding_x + button_width) * button_num + button_border_size,
                   height - padding_y - button_height + button_border_size,
                   button_width - button_border_size * 2,
                   button_height - button_border_size * 2);

    XDrawString(display,
                window,
                borderGC,
                width - (padding_x + button_width) * button_num + button_inset_x,
                height - padding_y - button_text_offset_y,
                label,
                strlen(label));
  }

  /* Is the mouse inside the given button */
  bool isInsideButton(const XEvent &e, uint button_num) const
  {
    return (
        (e.xmotion.y > int(height - padding_y - button_height)) &&
        (e.xmotion.y < int(height - padding_y)) &&
        (e.xmotion.x > int(width - (padding_x + button_width) * button_num)) &&
        (e.xmotion.x < int(width - padding_x - (padding_x + button_width) * (button_num - 1))));
  }
};

static void split(const char *text, const char *seps, char ***str, int *count)
{
  const char *tok;
  char *data;
  int i;
  *count = 0;

  data = strdup(text);
  for (tok = strtok(data, seps); tok != nullptr; tok = strtok(nullptr, seps)) {
    (*count)++;
  }
  free(data);

  data = strdup(text);
  *str = (char **)malloc(size_t(*count) * sizeof(char *));
  for (i = 0, tok = strtok(data, seps); tok != nullptr; tok = strtok(nullptr, seps), i++) {
    (*str)[i] = strdup(tok);
  }
  free(data);
}

GHOST_TSuccess GHOST_SystemX11::showMessageBox(const char *title,
                                               const char *message,
                                               const char *help_label,
                                               const char *continue_label,
                                               const char *link,
                                               GHOST_DialogOptions /*dialog_options*/) const
{
  char **text_splitted = nullptr;
  int textLines = 0;
  split(message, "\n", &text_splitted, &textLines);

  DialogData dialog_data;
  XSizeHints hints;

  Window window;
  XEvent e;
  int screen = DefaultScreen(display_);
  window = XCreateSimpleWindow(display_,
                               RootWindow(display_, screen),
                               0,
                               0,
                               dialog_data.width,
                               dialog_data.height,
                               1,
                               BlackPixel(display_, screen),
                               WhitePixel(display_, screen));

  /* Window Should not be resizable */
  {
    hints.flags = PSize | PMinSize | PMaxSize;
    hints.min_width = hints.max_width = hints.base_width = dialog_data.width;
    hints.min_height = hints.max_height = hints.base_height = dialog_data.height;
    XSetWMNormalHints(display_, window, &hints);
  }

  /* Set title */
  {
    Atom wm_Name = XInternAtom(display_, "_NET_WM_NAME", False);
    Atom utf8Str = XInternAtom(display_, "UTF8_STRING", False);

    Atom winType = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
    Atom typeDialog = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_DIALOG", False);

    XChangeProperty(display_,
                    window,
                    wm_Name,
                    utf8Str,
                    8,
                    PropModeReplace,
                    (const uchar *)title,
                    int(strlen(title)));

    XChangeProperty(
        display_, window, winType, XA_ATOM, 32, PropModeReplace, (uchar *)&typeDialog, 1);
  }

  /* Create buttons GC */
  XGCValues buttonBorderGCValues;
  buttonBorderGCValues.foreground = BlackPixel(display_, screen);
  buttonBorderGCValues.background = WhitePixel(display_, screen);
  XGCValues buttonGCValues;
  buttonGCValues.foreground = WhitePixel(display_, screen);
  buttonGCValues.background = BlackPixel(display_, screen);

  GC buttonBorderGC = XCreateGC(display_, window, GCForeground, &buttonBorderGCValues);
  GC buttonGC = XCreateGC(display_, window, GCForeground, &buttonGCValues);

  XSelectInput(display_, window, ExposureMask | ButtonPressMask | ButtonReleaseMask);
  XMapWindow(display_, window);

  const bool has_link = link && strlen(link);

  while (true) {
    XNextEvent(display_, &e);
    if (e.type == Expose) {
      for (int i = 0; i < textLines; i++) {
        XDrawString(display_,
                    window,
                    DefaultGC(display_, screen),
                    dialog_data.padding_x,
                    dialog_data.padding_x + (i + 1) * dialog_data.line_height,
                    text_splitted[i],
                    int(strlen(text_splitted[i])));
      }
      dialog_data.drawButton(display_, window, buttonBorderGC, buttonGC, 1, continue_label);
      if (has_link) {
        dialog_data.drawButton(display_, window, buttonBorderGC, buttonGC, 2, help_label);
      }
    }
    else if (e.type == ButtonRelease) {
      if (dialog_data.isInsideButton(e, 1)) {
        break;
      }
      if (dialog_data.isInsideButton(e, 2)) {
        if (has_link) {
          string cmd = "xdg-open \"" + string(link) + "\"";
          if (system(cmd.c_str()) != 0) {
            GHOST_PRINTF("GHOST_SystemX11::showMessageBox: Unable to run system command [%s]",
                         cmd.c_str());
          }
        }
        break;
      }
    }
  }

  for (int i = 0; i < textLines; i++) {
    free(text_splitted[i]);
  }
  free(text_splitted);

  XDestroyWindow(display_, window);
  XFreeGC(display_, buttonBorderGC);
  XFreeGC(display_, buttonGC);
  return GHOST_kSuccess;
}

/** \} */

#ifdef WITH_XDND
GHOST_TSuccess GHOST_SystemX11::pushDragDropEvent(GHOST_TEventType eventType,
                                                  GHOST_TDragnDropTypes draggedObjectType,
                                                  GHOST_IWindow *window,
                                                  int mouseX,
                                                  int mouseY,
                                                  void *data)
{
  GHOST_SystemX11 *system = ((GHOST_SystemX11 *)getSystem());

  /* Caller has no timestamp. */
  const uint64_t event_ms = system->getMilliSeconds();

  return system->pushEvent(new GHOST_EventDragnDrop(
      event_ms, eventType, draggedObjectType, window, mouseX, mouseY, data));
}
#endif
/**
 * These callbacks can be used for debugging, so we can break-point on an X11 error.
 *
 * Dummy function to get around IO Handler exiting if device invalid
 * Basically it will not crash blender now if you have a X device that
 * is configured but not plugged in.
 */
int GHOST_X11_ApplicationErrorHandler(Display *display, XErrorEvent *event)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  if (!system->isDebugEnabled()) {
    return 0;
  }

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
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  if (!system->isDebugEnabled()) {
    return 0;
  }

  fprintf(stderr, "Ignoring Xlib error: error IO\n");

  /* No exit! - but keep lint happy */
  return 0;
}

#ifdef WITH_X11_XINPUT

static bool is_filler_char(char c)
{
  return isspace(c) || ELEM(c, '_', '-', ';', ':');
}

/* These C functions are copied from Wine 3.12's `wintab.c` */
static bool match_token(const char *haystack, const char *needle)
{
  const char *h, *n;
  for (h = haystack; *h;) {
    while (*h && is_filler_char(*h)) {
      h++;
    }
    if (!*h) {
      break;
    }

    for (n = needle; *n && *h && tolower(*h) == tolower(*n); n++) {
      h++;
    }
    if (!*n && (is_filler_char(*h) || !*h)) {
      return true;
    }

    while (*h && !is_filler_char(*h)) {
      h++;
    }
  }
  return false;
}

/* Determining if an X device is a Tablet style device is an imperfect science.
 * We rely on common conventions around device names as well as the type reported
 * by WACOM tablets.  This code will likely need to be expanded for alternate tablet types
 *
 * WINTAB refers to any device that interacts with the tablet as a cursor,
 * (stylus, eraser, tablet mouse, airbrush, etc)
 * this is not to be confused with WACOM X11 configuration "cursor" device.
 * WACOM tablets X11 configuration "cursor" refers to its device slot (which we mirror with
 * our `gSysCursors`) for puck like devices (tablet mice essentially).
 */
static GHOST_TTabletMode tablet_mode_from_name(const char *name, const char *type)
{
  int i;
  static const char *tablet_stylus_whitelist[] = {"stylus", "wizardpen", "acecad", "pen", nullptr};

  static const char *type_blacklist[] = {"pad", "cursor", "touch", nullptr};

  /* Skip some known unsupported types. */
  for (i = 0; type_blacklist[i] != nullptr; i++) {
    if (type && (strcasecmp(type, type_blacklist[i]) == 0)) {
      return GHOST_kTabletModeNone;
    }
  }

  /* First check device type to avoid cases where name is "Pen and Eraser" and type is "ERASER" */
  for (i = 0; tablet_stylus_whitelist[i] != nullptr; i++) {
    if (type && match_token(type, tablet_stylus_whitelist[i])) {
      return GHOST_kTabletModeStylus;
    }
  }
  if (type && match_token(type, "eraser")) {
    return GHOST_kTabletModeEraser;
  }
  for (i = 0; tablet_stylus_whitelist[i] != nullptr; i++) {
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
  if (xinput_version_.present) {
    /* Close tablet devices. */
    clearXInputDevices();

    /* Install our error handler to override Xlib's termination behavior */
    GHOST_X11_ERROR_HANDLERS_OVERRIDE(handler_store);

    {
      int device_count;
      XDeviceInfo *device_info = XListInputDevices(display_, &device_count);

      for (int i = 0; i < device_count; ++i) {
        char *device_type = device_info[i].type ? XGetAtomName(display_, device_info[i].type) :
                                                  nullptr;
        GHOST_TTabletMode tablet_mode = tablet_mode_from_name(device_info[i].name, device_type);

        // printf("Tablet type:'%s', name:'%s', index:%d\n", device_type, device_info[i].name, i);

        if (device_type) {
          XFree((void *)device_type);
        }

        if (!ELEM(tablet_mode, GHOST_kTabletModeStylus, GHOST_kTabletModeEraser)) {
          continue;
        }

        GHOST_TabletX11 xtablet = {tablet_mode};
        xtablet.ID = device_info[i].id;
        xtablet.Device = XOpenDevice(display_, xtablet.ID);

        if (xtablet.Device != nullptr) {
          /* Find how many pressure levels tablet has */
          XAnyClassPtr ici = device_info[i].inputclassinfo;

          if (ici != nullptr) {
            for (int j = 0; j < device_info[i].num_classes; ++j) {
              if (ici->c_class == ValuatorClass) {
                XValuatorInfo *xvi = (XValuatorInfo *)ici;
                if (xvi->axes != nullptr) {
                  xtablet.PressureLevels = xvi->axes[2].max_value;

                  if (xvi->num_axes > 3) {
                    /* This is assuming that the tablet has the same tilt resolution in both
                     * positive and negative directions. It would be rather weird if it didn't. */
                    xtablet.XtiltLevels = xvi->axes[3].max_value;
                    xtablet.YtiltLevels = xvi->axes[4].max_value;
                  }
                  else {
                    xtablet.XtiltLevels = 0;
                    xtablet.YtiltLevels = 0;
                  }

                  break;
                }
              }

              ici = (XAnyClassPtr)(((char *)ici) + ici->length);
            }
          }

          xtablets_.push_back(xtablet);
        }
      }

      XFreeDeviceList(device_info);
    }

    GHOST_X11_ERROR_HANDLERS_RESTORE(handler_store);
  }
}

void GHOST_SystemX11::clearXInputDevices()
{
  for (GHOST_TabletX11 &xtablet : xtablets_) {
    if (xtablet.Device) {
      XCloseDevice(display_, xtablet.Device);
    }
  }

  xtablets_.clear();
}

#endif /* WITH_X11_XINPUT */
