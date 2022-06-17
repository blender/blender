/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_SystemWayland.h"
#include "GHOST_Event.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventDragnDrop.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventWheel.h"
#include "GHOST_TimerManager.h"
#include "GHOST_WindowManager.h"

#include "GHOST_ContextEGL.h"

#include <EGL/egl.h>
#include <wayland-egl.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "GHOST_WaylandCursorSettings.h"
#include <pointer-constraints-client-protocol.h>
#include <relative-pointer-client-protocol.h>
#include <tablet-client-protocol.h>
#include <wayland-cursor.h>
#include <xdg-output-client-protocol.h>

#include <xkbcommon/xkbcommon.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <mutex>

static GHOST_WindowWayland *window_from_surface(struct wl_surface *surface);

/* -------------------------------------------------------------------- */
/** \name Private Types & Defines
 * \{ */

/**
 * Selected input event code defines from `linux/input-event-codes.h`
 * We include some of the button input event codes here, since the header is
 * only available in more recent kernel versions. The event codes are used to
 * to differentiate from which mouse button an event comes from.
 */
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#define BTN_EXTRA 0x114
#define BTN_FORWARD 0x115
#define BTN_BACK 0x116
// #define BTN_TASK 0x117 /* UNUSED. */

/**
 * Tablet events, also from `linux/input-event-codes.h`.
 */
#define BTN_STYLUS 0x14b  /* Use as right-mouse. */
#define BTN_STYLUS2 0x14c /* Use as middle-mouse. */
/* NOTE(@campbellbarton): Map to an additional button (not sure which hardware uses this). */
#define BTN_STYLUS3 0x149

struct buffer_t {
  void *data = nullptr;
  size_t size = 0;
};

struct cursor_t {
  bool visible = false;
  struct wl_surface *wl_surface = nullptr;
  struct wl_buffer *wl_buffer = nullptr;
  struct wl_cursor_image wl_image = {0};
  struct wl_cursor_theme *wl_theme = nullptr;
  struct buffer_t *file_buffer = nullptr;
  int size = 0;
  std::string theme_name;
  /** Outputs on which the cursor is visible. */
  std::unordered_set<const output_t *> outputs;
  int scale = 1;
};

/**
 * A single tablet can have multiple tools (pen, eraser, brush... etc).
 * WAYLAND exposes tools via #zwp_tablet_tool_v2.
 * Since are no API's to access properties of the tool, store the values here.
 */
struct tablet_tool_input_t {
  struct input_t *input = nullptr;
  struct wl_surface *cursor_surface = nullptr;

  GHOST_TabletData data = GHOST_TABLET_DATA_NONE;
};

struct data_offer_t {
  std::unordered_set<std::string> types;
  uint32_t source_actions = 0;
  uint32_t dnd_action = 0;
  struct wl_data_offer *id = nullptr;
  std::atomic<bool> in_use = false;
  struct {
    /** Compatible with #input_t.xy coordinates. */
    wl_fixed_t xy[2] = {0, 0};
  } dnd;
};

struct data_source_t {
  struct wl_data_source *data_source = nullptr;
  char *buffer_out = nullptr;
};

struct key_repeat_payload_t {
  GHOST_SystemWayland *system = nullptr;
  GHOST_IWindow *window = nullptr;
  GHOST_TEventKeyData key_data = {GHOST_kKeyUnknown};
};

struct input_t {
  GHOST_SystemWayland *system = nullptr;

  std::string name;
  struct wl_seat *wl_seat = nullptr;
  struct wl_pointer *wl_pointer = nullptr;
  struct wl_keyboard *wl_keyboard = nullptr;
  struct zwp_tablet_seat_v2 *tablet_seat = nullptr;

  /** All currently active tablet tools (needed for changing the cursor). */
  std::unordered_set<zwp_tablet_tool_v2 *> tablet_tools;

  uint32_t pointer_serial = 0;
  uint32_t tablet_serial = 0;

  /** Use to check if the last cursor input was tablet or pointer. */
  uint32_t cursor_serial = 0;

  /**
   * High precision mouse coordinates (pointer or tablet).
   *
   * The following example converts these values to screen coordinates.
   * \code{.cc}
   * const wl_fixed_t scale = win->scale();
   * const int event_xy[2] = {
   *   wl_fixed_to_int(scale * input->xy[0]),
   *   wl_fixed_to_int(scale * input->xy[1]),
   * };
   * \endcode
   */
  wl_fixed_t xy[2] = {0, 0};
  GHOST_Buttons buttons = GHOST_Buttons();
  struct cursor_t cursor;

  struct zwp_relative_pointer_v1 *relative_pointer = nullptr;
  struct zwp_locked_pointer_v1 *locked_pointer = nullptr;
  struct zwp_confined_pointer_v1 *confined_pointer = nullptr;

  struct xkb_context *xkb_context = nullptr;
  struct xkb_state *xkb_state = nullptr;
  struct {
    /** Key repetition in character per second. */
    int32_t rate = 0;
    /** Time (milliseconds) after which to start repeating keys. */
    int32_t delay = 0;
    /** Timer for key repeats. */
    GHOST_ITimerTask *timer = nullptr;
  } key_repeat;

  struct wl_surface *focus_tablet = nullptr;
  struct wl_surface *focus_pointer = nullptr;
  struct wl_surface *focus_keyboard = nullptr;
  struct wl_surface *focus_dnd = nullptr;

  struct wl_data_device *data_device = nullptr;
  /** Drag & Drop. */
  struct data_offer_t *data_offer_dnd = nullptr;
  std::mutex data_offer_dnd_mutex;

  /** Copy & Paste. */
  struct data_offer_t *data_offer_copy_paste = nullptr;
  std::mutex data_offer_copy_paste_mutex;

  struct data_source_t *data_source = nullptr;
  std::mutex data_source_mutex;

  /** Last device that was active. */
  uint32_t data_source_serial = 0;
};

struct display_t {
  GHOST_SystemWayland *system = nullptr;

  struct wl_display *display = nullptr;
  struct wl_compositor *compositor = nullptr;
  struct xdg_wm_base *xdg_shell = nullptr;
  struct zxdg_decoration_manager_v1 *xdg_decoration_manager = nullptr;
  struct zxdg_output_manager_v1 *xdg_output_manager = nullptr;
  struct wl_shm *shm = nullptr;
  std::vector<output_t *> outputs;
  std::vector<input_t *> inputs;
  struct {
    std::string theme;
    int size = 0;
  } cursor;
  struct wl_data_device_manager *data_device_manager = nullptr;
  struct zwp_tablet_manager_v2 *tablet_manager = nullptr;
  struct zwp_relative_pointer_manager_v1 *relative_pointer_manager = nullptr;
  struct zwp_pointer_constraints_v1 *pointer_constraints = nullptr;

  std::vector<struct wl_surface *> os_surfaces;
  std::vector<struct wl_egl_window *> os_egl_windows;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private Utility Functions
 * \{ */

static GHOST_WindowManager *window_manager = nullptr;

/** Check this lock before accessing `GHOST_SystemWayland::selection` from a thread. */
static std::mutex system_selection_mutex;

/**
 * Callback for WAYLAND to run when there is an error.
 *
 * \note It's useful to set a break-point on this function as some errors are fatal
 * (for all intents and purposes) but don't crash the process.
 */
static void ghost_wayland_log_handler(const char *msg, va_list arg)
{
  fprintf(stderr, "GHOST/Wayland: ");
  vfprintf(stderr, msg, arg); /* Includes newline. */

  GHOST_TBacktraceFn backtrace_fn = GHOST_ISystem::getBacktraceFn();
  if (backtrace_fn) {
    backtrace_fn(stderr); /* Includes newline. */
  }
}

static void display_destroy(display_t *d)
{
  if (d->data_device_manager) {
    wl_data_device_manager_destroy(d->data_device_manager);
  }

  if (d->tablet_manager) {
    zwp_tablet_manager_v2_destroy(d->tablet_manager);
  }

  for (output_t *output : d->outputs) {
    wl_output_destroy(output->wl_output);
    delete output;
  }

  for (input_t *input : d->inputs) {

    /* First handle members that require locking.
     * While highly unlikely, it's possible they are being used while this function runs. */
    {
      std::lock_guard lock{input->data_source_mutex};
      if (input->data_source) {
        free(input->data_source->buffer_out);
        if (input->data_source->data_source) {
          wl_data_source_destroy(input->data_source->data_source);
        }
        delete input->data_source;
      }
    }

    {
      std::lock_guard lock{input->data_offer_dnd_mutex};
      if (input->data_offer_dnd) {
        wl_data_offer_destroy(input->data_offer_dnd->id);
        delete input->data_offer_dnd;
      }
    }

    {
      std::lock_guard lock{input->data_offer_copy_paste_mutex};
      if (input->data_offer_copy_paste) {
        wl_data_offer_destroy(input->data_offer_copy_paste->id);
        delete input->data_offer_copy_paste;
      }
    }

    if (input->data_device) {
      wl_data_device_release(input->data_device);
    }
    if (input->wl_pointer) {
      if (input->cursor.file_buffer) {
        munmap(input->cursor.file_buffer->data, input->cursor.file_buffer->size);
        delete input->cursor.file_buffer;
      }
      if (input->cursor.wl_surface) {
        wl_surface_destroy(input->cursor.wl_surface);
      }
      if (input->cursor.wl_theme) {
        wl_cursor_theme_destroy(input->cursor.wl_theme);
      }
      if (input->wl_pointer) {
        wl_pointer_destroy(input->wl_pointer);
      }
    }
    if (input->wl_keyboard) {
      if (input->key_repeat.timer) {
        delete static_cast<key_repeat_payload_t *>(input->key_repeat.timer->getUserData());
        input->system->removeTimer(input->key_repeat.timer);
        input->key_repeat.timer = nullptr;
      }
      wl_keyboard_destroy(input->wl_keyboard);
    }
    if (input->xkb_state) {
      xkb_state_unref(input->xkb_state);
    }
    if (input->xkb_context) {
      xkb_context_unref(input->xkb_context);
    }
    wl_seat_destroy(input->wl_seat);
    delete input;
  }

  if (d->shm) {
    wl_shm_destroy(d->shm);
  }

  if (d->relative_pointer_manager) {
    zwp_relative_pointer_manager_v1_destroy(d->relative_pointer_manager);
  }

  if (d->pointer_constraints) {
    zwp_pointer_constraints_v1_destroy(d->pointer_constraints);
  }

  for (wl_egl_window *os_egl_window : d->os_egl_windows) {
    wl_egl_window_destroy(os_egl_window);
  }

  for (wl_surface *os_surface : d->os_surfaces) {
    wl_surface_destroy(os_surface);
  }

  if (d->compositor) {
    wl_compositor_destroy(d->compositor);
  }

  if (d->xdg_decoration_manager) {
    zxdg_decoration_manager_v1_destroy(d->xdg_decoration_manager);
  }

  if (d->xdg_shell) {
    xdg_wm_base_destroy(d->xdg_shell);
  }

  if (eglGetDisplay) {
    ::eglTerminate(eglGetDisplay(EGLNativeDisplayType(d->display)));
  }

  if (d->display) {
    wl_display_disconnect(d->display);
  }

  delete d;
}

static GHOST_TKey xkb_map_gkey(const xkb_keysym_t &sym)
{

  GHOST_TKey gkey;
  if (sym >= XKB_KEY_0 && sym <= XKB_KEY_9) {
    gkey = GHOST_TKey(sym);
  }
  else if (sym >= XKB_KEY_KP_0 && sym <= XKB_KEY_KP_9) {
    gkey = GHOST_TKey(GHOST_kKeyNumpad0 + sym - XKB_KEY_KP_0);
  }
  else if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z) {
    gkey = GHOST_TKey(sym);
  }
  else if (sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
    gkey = GHOST_TKey(sym - XKB_KEY_a + XKB_KEY_A);
  }
  else if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F24) {
    gkey = GHOST_TKey(GHOST_kKeyF1 + sym - XKB_KEY_F1);
  }
  else {

#define GXMAP(k, x, y) \
  case x: \
    k = y; \
    break

    switch (sym) {
      GXMAP(gkey, XKB_KEY_BackSpace, GHOST_kKeyBackSpace);
      GXMAP(gkey, XKB_KEY_Tab, GHOST_kKeyTab);
      GXMAP(gkey, XKB_KEY_Linefeed, GHOST_kKeyLinefeed);
      GXMAP(gkey, XKB_KEY_Clear, GHOST_kKeyClear);
      GXMAP(gkey, XKB_KEY_Return, GHOST_kKeyEnter);

      GXMAP(gkey, XKB_KEY_Escape, GHOST_kKeyEsc);
      GXMAP(gkey, XKB_KEY_space, GHOST_kKeySpace);
      GXMAP(gkey, XKB_KEY_apostrophe, GHOST_kKeyQuote);
      GXMAP(gkey, XKB_KEY_comma, GHOST_kKeyComma);
      GXMAP(gkey, XKB_KEY_minus, GHOST_kKeyMinus);
      GXMAP(gkey, XKB_KEY_plus, GHOST_kKeyPlus);
      GXMAP(gkey, XKB_KEY_period, GHOST_kKeyPeriod);
      GXMAP(gkey, XKB_KEY_slash, GHOST_kKeySlash);

      GXMAP(gkey, XKB_KEY_semicolon, GHOST_kKeySemicolon);
      GXMAP(gkey, XKB_KEY_equal, GHOST_kKeyEqual);

      GXMAP(gkey, XKB_KEY_bracketleft, GHOST_kKeyLeftBracket);
      GXMAP(gkey, XKB_KEY_bracketright, GHOST_kKeyRightBracket);
      GXMAP(gkey, XKB_KEY_backslash, GHOST_kKeyBackslash);
      GXMAP(gkey, XKB_KEY_grave, GHOST_kKeyAccentGrave);

      GXMAP(gkey, XKB_KEY_Shift_L, GHOST_kKeyLeftShift);
      GXMAP(gkey, XKB_KEY_Shift_R, GHOST_kKeyRightShift);
      GXMAP(gkey, XKB_KEY_Control_L, GHOST_kKeyLeftControl);
      GXMAP(gkey, XKB_KEY_Control_R, GHOST_kKeyRightControl);
      GXMAP(gkey, XKB_KEY_Alt_L, GHOST_kKeyLeftAlt);
      GXMAP(gkey, XKB_KEY_Alt_R, GHOST_kKeyRightAlt);
      GXMAP(gkey, XKB_KEY_Super_L, GHOST_kKeyOS);
      GXMAP(gkey, XKB_KEY_Super_R, GHOST_kKeyOS);
      GXMAP(gkey, XKB_KEY_Menu, GHOST_kKeyApp);

      GXMAP(gkey, XKB_KEY_Caps_Lock, GHOST_kKeyCapsLock);
      GXMAP(gkey, XKB_KEY_Num_Lock, GHOST_kKeyNumLock);
      GXMAP(gkey, XKB_KEY_Scroll_Lock, GHOST_kKeyScrollLock);

      GXMAP(gkey, XKB_KEY_Left, GHOST_kKeyLeftArrow);
      GXMAP(gkey, XKB_KEY_Right, GHOST_kKeyRightArrow);
      GXMAP(gkey, XKB_KEY_Up, GHOST_kKeyUpArrow);
      GXMAP(gkey, XKB_KEY_Down, GHOST_kKeyDownArrow);

      GXMAP(gkey, XKB_KEY_Print, GHOST_kKeyPrintScreen);
      GXMAP(gkey, XKB_KEY_Pause, GHOST_kKeyPause);

      GXMAP(gkey, XKB_KEY_Insert, GHOST_kKeyInsert);
      GXMAP(gkey, XKB_KEY_Delete, GHOST_kKeyDelete);
      GXMAP(gkey, XKB_KEY_Home, GHOST_kKeyHome);
      GXMAP(gkey, XKB_KEY_End, GHOST_kKeyEnd);
      GXMAP(gkey, XKB_KEY_Page_Up, GHOST_kKeyUpPage);
      GXMAP(gkey, XKB_KEY_Page_Down, GHOST_kKeyDownPage);

      GXMAP(gkey, XKB_KEY_KP_Decimal, GHOST_kKeyNumpadPeriod);
      GXMAP(gkey, XKB_KEY_KP_Enter, GHOST_kKeyNumpadEnter);
      GXMAP(gkey, XKB_KEY_KP_Add, GHOST_kKeyNumpadPlus);
      GXMAP(gkey, XKB_KEY_KP_Subtract, GHOST_kKeyNumpadMinus);
      GXMAP(gkey, XKB_KEY_KP_Multiply, GHOST_kKeyNumpadAsterisk);
      GXMAP(gkey, XKB_KEY_KP_Divide, GHOST_kKeyNumpadSlash);

      GXMAP(gkey, XKB_KEY_XF86AudioPlay, GHOST_kKeyMediaPlay);
      GXMAP(gkey, XKB_KEY_XF86AudioStop, GHOST_kKeyMediaStop);
      GXMAP(gkey, XKB_KEY_XF86AudioPrev, GHOST_kKeyMediaFirst);
      GXMAP(gkey, XKB_KEY_XF86AudioNext, GHOST_kKeyMediaLast);
      default:
        GHOST_PRINT("unhandled key: " << std::hex << std::showbase << sym << std::dec << " ("
                                      << sym << ")" << std::endl);
        gkey = GHOST_kKeyUnknown;
    }
#undef GXMAP
  }

  return gkey;
}

static GHOST_TTabletMode tablet_tool_map_type(enum zwp_tablet_tool_v2_type wl_tablet_tool_type)
{
  switch (wl_tablet_tool_type) {
    case ZWP_TABLET_TOOL_V2_TYPE_ERASER: {
      return GHOST_kTabletModeEraser;
    }
    case ZWP_TABLET_TOOL_V2_TYPE_PEN:
    case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
    case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
    case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
    case ZWP_TABLET_TOOL_V2_TYPE_FINGER:
    case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
    case ZWP_TABLET_TOOL_V2_TYPE_LENS: {
      return GHOST_kTabletModeStylus;
    }
  }

  GHOST_PRINT("unknown tablet tool: " << wl_tablet_tool_type << std::endl);
  return GHOST_kTabletModeStylus;
}

static const int default_cursor_size = 24;

static const std::unordered_map<GHOST_TStandardCursor, std::string> cursors = {
    {GHOST_kStandardCursorDefault, "left_ptr"},
    {GHOST_kStandardCursorRightArrow, "right_ptr"},
    {GHOST_kStandardCursorLeftArrow, "left_ptr"},
    {GHOST_kStandardCursorInfo, ""},
    {GHOST_kStandardCursorDestroy, ""},
    {GHOST_kStandardCursorHelp, "question_arrow"},
    {GHOST_kStandardCursorWait, "watch"},
    {GHOST_kStandardCursorText, "xterm"},
    {GHOST_kStandardCursorCrosshair, "crosshair"},
    {GHOST_kStandardCursorCrosshairA, ""},
    {GHOST_kStandardCursorCrosshairB, ""},
    {GHOST_kStandardCursorCrosshairC, ""},
    {GHOST_kStandardCursorPencil, ""},
    {GHOST_kStandardCursorUpArrow, "sb_up_arrow"},
    {GHOST_kStandardCursorDownArrow, "sb_down_arrow"},
    {GHOST_kStandardCursorVerticalSplit, ""},
    {GHOST_kStandardCursorHorizontalSplit, ""},
    {GHOST_kStandardCursorEraser, ""},
    {GHOST_kStandardCursorKnife, ""},
    {GHOST_kStandardCursorEyedropper, ""},
    {GHOST_kStandardCursorZoomIn, ""},
    {GHOST_kStandardCursorZoomOut, ""},
    {GHOST_kStandardCursorMove, "move"},
    {GHOST_kStandardCursorNSEWScroll, ""},
    {GHOST_kStandardCursorNSScroll, ""},
    {GHOST_kStandardCursorEWScroll, ""},
    {GHOST_kStandardCursorStop, ""},
    {GHOST_kStandardCursorUpDown, "sb_v_double_arrow"},
    {GHOST_kStandardCursorLeftRight, "sb_h_double_arrow"},
    {GHOST_kStandardCursorTopSide, "top_side"},
    {GHOST_kStandardCursorBottomSide, "bottom_side"},
    {GHOST_kStandardCursorLeftSide, "left_side"},
    {GHOST_kStandardCursorRightSide, "right_side"},
    {GHOST_kStandardCursorTopLeftCorner, "top_left_corner"},
    {GHOST_kStandardCursorTopRightCorner, "top_right_corner"},
    {GHOST_kStandardCursorBottomRightCorner, "bottom_right_corner"},
    {GHOST_kStandardCursorBottomLeftCorner, "bottom_left_corner"},
    {GHOST_kStandardCursorCopy, "copy"},
};

static constexpr const char *mime_text_plain = "text/plain";
static constexpr const char *mime_text_utf8 = "text/plain;charset=utf-8";
static constexpr const char *mime_text_uri = "text/uri-list";

static const std::unordered_map<std::string, GHOST_TDragnDropTypes> mime_dnd = {
    {mime_text_plain, GHOST_kDragnDropTypeString},
    {mime_text_utf8, GHOST_kDragnDropTypeString},
    {mime_text_uri, GHOST_kDragnDropTypeFilenames},
};

static const std::vector<std::string> mime_preference_order = {
    mime_text_uri,
    mime_text_utf8,
    mime_text_plain,
};

static const std::vector<std::string> mime_send = {
    "UTF8_STRING",
    "COMPOUND_TEXT",
    "TEXT",
    "STRING",
    "text/plain;charset=utf-8",
    "text/plain",
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Relative Motion), #zwp_relative_pointer_v1_listener
 *
 * These callbacks are registered for Wayland interfaces and called when
 * an event is received from the compositor.
 * \{ */

static void relative_pointer_handle_relative_motion(
    void *data,
    struct zwp_relative_pointer_v1 * /*zwp_relative_pointer_v1*/,
    uint32_t /*utime_hi*/,
    uint32_t /*utime_lo*/,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t /*dx_unaccel*/,
    wl_fixed_t /*dy_unaccel*/)
{
  input_t *input = static_cast<input_t *>(data);
  GHOST_WindowWayland *win = window_from_surface(input->focus_pointer);
  if (!win) {
    return;
  }
  const wl_fixed_t scale = win->scale();
  input->xy[0] += dx / scale;
  input->xy[1] += dy / scale;

  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 win,
                                                 wl_fixed_to_int(scale * input->xy[0]),
                                                 wl_fixed_to_int(scale * input->xy[1]),
                                                 GHOST_TABLET_DATA_NONE));
}

static const zwp_relative_pointer_v1_listener relative_pointer_listener = {
    relative_pointer_handle_relative_motion,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Source), #wl_data_source_listener
 * \{ */

static void dnd_events(const input_t *const input, const GHOST_TEventType event)
{
  /* NOTE: `input->data_offer_dnd_mutex` must already be locked. */
  const uint64_t time = input->system->getMilliSeconds();
  GHOST_WindowWayland *const win = static_cast<GHOST_WindowWayland *>(
      wl_surface_get_user_data(input->focus_dnd));
  if (!win) {
    return;
  }
  const wl_fixed_t scale = win->scale();
  const int event_xy[2] = {
      wl_fixed_to_int(scale * input->data_offer_dnd->dnd.xy[0]),
      wl_fixed_to_int(scale * input->data_offer_dnd->dnd.xy[1]),
  };

  for (const std::string &type : mime_preference_order) {
    input->system->pushEvent(new GHOST_EventDragnDrop(
        time, event, mime_dnd.at(type), win, event_xy[0], event_xy[1], nullptr));
  }
}

static std::string read_pipe(data_offer_t *data_offer,
                             const std::string mime_receive,
                             std::mutex *mutex)
{
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return {};
  }
  wl_data_offer_receive(data_offer->id, mime_receive.c_str(), pipefd[1]);
  close(pipefd[1]);

  data_offer->in_use.store(false);

  if (mutex) {
    mutex->unlock();
  }
  /* WARNING: `data_offer` may be freed from now on. */

  std::string data;
  ssize_t len;
  char buffer[4096];
  while ((len = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
    data.insert(data.end(), buffer, buffer + len);
  }
  close(pipefd[0]);

  return data;
}

/**
 * A target accepts an offered mime type.
 *
 * Sent when a target accepts pointer_focus or motion events. If
 * a target does not accept any of the offered types, type is nullptr.
 */
static void data_source_handle_target(void * /*data*/,
                                      struct wl_data_source * /*wl_data_source*/,
                                      const char * /*mime_type*/)
{
  /* pass */
}

static void data_source_handle_send(void *data,
                                    struct wl_data_source * /*wl_data_source*/,
                                    const char * /*mime_type*/,
                                    int32_t fd)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_source_mutex};

  const char *const buffer = input->data_source->buffer_out;
  if (write(fd, buffer, strlen(buffer)) < 0) {
    GHOST_PRINT("error writing to clipboard: " << std::strerror(errno) << std::endl);
  }
  close(fd);
}

static void data_source_handle_cancelled(void * /*data*/, struct wl_data_source *wl_data_source)
{
  wl_data_source_destroy(wl_data_source);
}

/**
 * The drag-and-drop operation physically finished.
 *
 * The user performed the drop action. This event does not
 * indicate acceptance, #wl_data_source.cancelled may still be
 * emitted afterwards if the drop destination does not accept any mime type.
 */
static void data_source_handle_dnd_drop_performed(void * /*data*/,
                                                  struct wl_data_source * /*wl_data_source*/)
{
  /* pass */
}

/**
 * The drag-and-drop operation concluded.
 *
 * The drop destination finished interoperating with this data
 * source, so the client is now free to destroy this data source
 * and free all associated data.
 */
static void data_source_handle_dnd_finished(void * /*data*/,
                                            struct wl_data_source * /*wl_data_source*/)
{
  /* pass */
}

/**
 * Notify the selected action.
 *
 * This event indicates the action selected by the compositor
 * after matching the source/destination side actions. Only one
 * action (or none) will be offered here.
 */
static void data_source_handle_action(void * /*data*/,
                                      struct wl_data_source * /*wl_data_source*/,
                                      uint32_t /*dnd_action*/)
{
  /* pass */
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_handle_target,
    data_source_handle_send,
    data_source_handle_cancelled,
    data_source_handle_dnd_drop_performed,
    data_source_handle_dnd_finished,
    data_source_handle_action,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Offer), #wl_data_offer_listener
 * \{ */

static void data_offer_handle_offer(void *data,
                                    struct wl_data_offer * /*wl_data_offer*/,
                                    const char *mime_type)
{
  static_cast<data_offer_t *>(data)->types.insert(mime_type);
}

static void data_offer_handle_source_actions(void *data,
                                             struct wl_data_offer * /*wl_data_offer*/,
                                             uint32_t source_actions)
{
  static_cast<data_offer_t *>(data)->source_actions = source_actions;
}

static void data_offer_handle_action(void *data,
                                     struct wl_data_offer * /*wl_data_offer*/,
                                     uint32_t dnd_action)
{
  static_cast<data_offer_t *>(data)->dnd_action = dnd_action;
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_handle_source_actions,
    data_offer_handle_action,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Device), #wl_data_device_listener
 * \{ */

static void data_device_handle_data_offer(void * /*data*/,
                                          struct wl_data_device * /*wl_data_device*/,
                                          struct wl_data_offer *id)
{
  data_offer_t *data_offer = new data_offer_t;
  data_offer->id = id;
  wl_data_offer_add_listener(id, &data_offer_listener, data_offer);
}

static void data_device_handle_enter(void *data,
                                     struct wl_data_device * /*wl_data_device*/,
                                     uint32_t serial,
                                     struct wl_surface *surface,
                                     wl_fixed_t x,
                                     wl_fixed_t y,
                                     struct wl_data_offer *id)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  input->data_offer_dnd = static_cast<data_offer_t *>(wl_data_offer_get_user_data(id));
  data_offer_t *data_offer = input->data_offer_dnd;

  data_offer->in_use.store(true);
  data_offer->dnd.xy[0] = x;
  data_offer->dnd.xy[1] = y;

  wl_data_offer_set_actions(id,
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);

  for (const std::string &type : mime_preference_order) {
    wl_data_offer_accept(id, serial, type.c_str());
  }

  input->focus_dnd = surface;
  dnd_events(input, GHOST_kEventDraggingEntered);
}

static void data_device_handle_leave(void *data, struct wl_data_device * /*wl_data_device*/)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  dnd_events(input, GHOST_kEventDraggingExited);
  input->focus_dnd = nullptr;

  if (input->data_offer_dnd && !input->data_offer_dnd->in_use.load()) {
    wl_data_offer_destroy(input->data_offer_dnd->id);
    delete input->data_offer_dnd;
    input->data_offer_dnd = nullptr;
  }
}

static void data_device_handle_motion(void *data,
                                      struct wl_data_device * /*wl_data_device*/,
                                      uint32_t /*time*/,
                                      wl_fixed_t x,
                                      wl_fixed_t y)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  input->data_offer_dnd->dnd.xy[0] = x;
  input->data_offer_dnd->dnd.xy[1] = y;

  dnd_events(input, GHOST_kEventDraggingUpdated);
}

static void data_device_handle_drop(void *data, struct wl_data_device * /*wl_data_device*/)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  data_offer_t *data_offer = input->data_offer_dnd;

  const std::string mime_receive = *std::find_first_of(mime_preference_order.begin(),
                                                       mime_preference_order.end(),
                                                       data_offer->types.begin(),
                                                       data_offer->types.end());

  auto read_uris_fn = [](input_t *const input,
                         data_offer_t *data_offer,
                         wl_surface *surface,
                         const std::string mime_receive) {
    const wl_fixed_t xy[2] = {data_offer->dnd.xy[0], data_offer->dnd.xy[1]};

    const std::string data = read_pipe(data_offer, mime_receive, nullptr);

    wl_data_offer_finish(data_offer->id);
    wl_data_offer_destroy(data_offer->id);

    delete data_offer;
    data_offer = nullptr;

    GHOST_SystemWayland *const system = input->system;

    if (mime_receive == mime_text_uri) {
      static constexpr const char *file_proto = "file://";
      static constexpr const char *crlf = "\r\n";

      GHOST_WindowWayland *win = window_from_surface(surface);
      GHOST_ASSERT(win != nullptr, "Unable to find window for drop event from surface");

      std::vector<std::string> uris;

      size_t pos = 0;
      while (true) {
        pos = data.find(file_proto, pos);
        const size_t start = pos + sizeof(file_proto) - 1;
        pos = data.find(crlf, pos);
        const size_t end = pos;

        if (pos == std::string::npos) {
          break;
        }
        uris.push_back(data.substr(start, end - start));
      }

      GHOST_TStringArray *flist = static_cast<GHOST_TStringArray *>(
          malloc(sizeof(GHOST_TStringArray)));
      flist->count = int(uris.size());
      flist->strings = static_cast<uint8_t **>(malloc(uris.size() * sizeof(uint8_t *)));
      for (size_t i = 0; i < uris.size(); i++) {
        flist->strings[i] = static_cast<uint8_t *>(malloc((uris[i].size() + 1) * sizeof(uint8_t)));
        memcpy(flist->strings[i], uris[i].data(), uris[i].size() + 1);
      }

      const wl_fixed_t scale = win->scale();
      system->pushEvent(new GHOST_EventDragnDrop(system->getMilliSeconds(),
                                                 GHOST_kEventDraggingDropDone,
                                                 GHOST_kDragnDropTypeFilenames,
                                                 win,
                                                 wl_fixed_to_int(scale * xy[0]),
                                                 wl_fixed_to_int(scale * xy[1]),
                                                 flist));
    }
    else if (mime_receive == mime_text_plain || mime_receive == mime_text_utf8) {
      /* TODO: enable use of internal functions 'txt_insert_buf' and
       * 'text_update_edited' to behave like dropped text was pasted. */
    }
    wl_display_roundtrip(system->display());
  };

  /* Pass in `input->focus_dnd` instead of accessing it from `input` since the leave callback
   * (#data_device_leave) will clear the value once this function starts. */
  std::thread read_thread(read_uris_fn, input, data_offer, input->focus_dnd, mime_receive);
  read_thread.detach();
}

static void data_device_handle_selection(void *data,
                                         struct wl_data_device * /*wl_data_device*/,
                                         struct wl_data_offer *id)
{
  input_t *input = static_cast<input_t *>(data);

  std::lock_guard lock{input->data_offer_copy_paste_mutex};

  data_offer_t *data_offer = input->data_offer_copy_paste;

  /* Delete old data offer. */
  if (data_offer != nullptr) {
    wl_data_offer_destroy(data_offer->id);
    delete data_offer;
    data_offer = nullptr;
  }

  if (id == nullptr) {
    return;
  }

  /* Get new data offer. */
  data_offer = static_cast<data_offer_t *>(wl_data_offer_get_user_data(id));
  input->data_offer_copy_paste = data_offer;

  auto read_selection_fn = [](input_t *input) {
    GHOST_SystemWayland *const system = input->system;
    input->data_offer_copy_paste_mutex.lock();

    data_offer_t *data_offer = input->data_offer_copy_paste;
    std::string mime_receive;
    for (const std::string type : {mime_text_utf8, mime_text_plain}) {
      if (data_offer->types.count(type)) {
        mime_receive = type;
        break;
      }
    }
    const std::string data = read_pipe(
        data_offer, mime_receive, &input->data_offer_copy_paste_mutex);

    {
      std::lock_guard lock{system_selection_mutex};
      system->setSelection(data);
    }
  };

  std::thread read_thread(read_selection_fn, input);
  read_thread.detach();
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Buffer), #wl_buffer_listener
 * \{ */

static void cursor_buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
  cursor_t *cursor = static_cast<cursor_t *>(data);

  wl_buffer_destroy(wl_buffer);

  if (wl_buffer == cursor->wl_buffer) {
    /* the mapped buffer was from a custom cursor */
    cursor->wl_buffer = nullptr;
  }
}

const struct wl_buffer_listener cursor_buffer_listener = {
    cursor_buffer_handle_release,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Surface), #wl_surface_listener
 * \{ */

static GHOST_WindowWayland *window_from_surface(struct wl_surface *surface)
{
  if (surface) {
    for (GHOST_IWindow *iwin : window_manager->getWindows()) {
      GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(iwin);
      if (surface == win->surface()) {
        return win;
      }
    }
  }
  return nullptr;
}

static bool update_cursor_scale(cursor_t &cursor, wl_shm *shm)
{
  int scale = 0;
  for (const output_t *output : cursor.outputs) {
    if (output->scale > scale) {
      scale = output->scale;
    }
  }

  if (scale > 0 && cursor.scale != scale) {
    cursor.scale = scale;
    wl_surface_set_buffer_scale(cursor.wl_surface, scale);
    wl_cursor_theme_destroy(cursor.wl_theme);
    cursor.wl_theme = wl_cursor_theme_load(cursor.theme_name.c_str(), scale * cursor.size, shm);
    return true;
  }
  return false;
}

static void cursor_surface_handle_enter(void *data,
                                        struct wl_surface * /*wl_surface*/,
                                        struct wl_output *output)
{
  input_t *input = static_cast<input_t *>(data);
  for (const output_t *reg_output : input->system->outputs()) {
    if (reg_output->wl_output == output) {
      input->cursor.outputs.insert(reg_output);
    }
  }
  update_cursor_scale(input->cursor, input->system->shm());
}

static void cursor_surface_handle_leave(void *data,
                                        struct wl_surface * /*wl_surface*/,
                                        struct wl_output *output)
{
  input_t *input = static_cast<input_t *>(data);
  for (const output_t *reg_output : input->system->outputs()) {
    if (reg_output->wl_output == output) {
      input->cursor.outputs.erase(reg_output);
    }
  }
  update_cursor_scale(input->cursor, input->system->shm());
}

struct wl_surface_listener cursor_surface_listener = {
    cursor_surface_handle_enter,
    cursor_surface_handle_leave,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Pointer), #wl_pointer_listener
 * \{ */

static void pointer_handle_enter(void *data,
                                 struct wl_pointer * /*wl_pointer*/,
                                 uint32_t serial,
                                 struct wl_surface *surface,
                                 wl_fixed_t surface_x,
                                 wl_fixed_t surface_y)
{
  GHOST_WindowWayland *win = window_from_surface(surface);
  if (!win) {
    return;
  }

  win->activate();

  input_t *input = static_cast<input_t *>(data);
  input->pointer_serial = serial;
  input->cursor_serial = serial;
  input->xy[0] = surface_x;
  input->xy[1] = surface_y;
  input->focus_pointer = surface;

  win->setCursorShape(win->getCursorShape());

  const wl_fixed_t scale = win->scale();
  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 static_cast<GHOST_WindowWayland *>(win),
                                                 wl_fixed_to_int(scale * input->xy[0]),
                                                 wl_fixed_to_int(scale * input->xy[1]),
                                                 GHOST_TABLET_DATA_NONE));
}

static void pointer_handle_leave(void *data,
                                 struct wl_pointer * /*wl_pointer*/,
                                 uint32_t /*serial*/,
                                 struct wl_surface *surface)
{
  GHOST_IWindow *win = window_from_surface(surface);
  if (!win) {
    return;
  }

  static_cast<input_t *>(data)->focus_pointer = nullptr;
  static_cast<GHOST_WindowWayland *>(win)->deactivate();
}

static void pointer_handle_motion(void *data,
                                  struct wl_pointer * /*wl_pointer*/,
                                  uint32_t /*time*/,
                                  wl_fixed_t surface_x,
                                  wl_fixed_t surface_y)
{
  input_t *input = static_cast<input_t *>(data);
  GHOST_WindowWayland *win = window_from_surface(input->focus_pointer);
  if (!win) {
    return;
  }

  input->xy[0] = surface_x;
  input->xy[1] = surface_y;

  const wl_fixed_t scale = win->scale();
  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 win,
                                                 wl_fixed_to_int(scale * input->xy[0]),
                                                 wl_fixed_to_int(scale * input->xy[1]),
                                                 GHOST_TABLET_DATA_NONE));
}

static void pointer_handle_button(void *data,
                                  struct wl_pointer * /*wl_pointer*/,
                                  uint32_t serial,
                                  uint32_t /*time*/,
                                  uint32_t button,
                                  uint32_t state)
{
  input_t *input = static_cast<input_t *>(data);
  GHOST_IWindow *win = window_from_surface(input->focus_pointer);
  if (!win) {
    return;
  }

  GHOST_TEventType etype = GHOST_kEventUnknown;
  switch (state) {
    case WL_POINTER_BUTTON_STATE_RELEASED:
      etype = GHOST_kEventButtonUp;
      break;
    case WL_POINTER_BUTTON_STATE_PRESSED:
      etype = GHOST_kEventButtonDown;
      break;
  }

  GHOST_TButtonMask ebutton = GHOST_kButtonMaskLeft;
  switch (button) {
    case BTN_LEFT:
      ebutton = GHOST_kButtonMaskLeft;
      break;
    case BTN_MIDDLE:
      ebutton = GHOST_kButtonMaskMiddle;
      break;
    case BTN_RIGHT:
      ebutton = GHOST_kButtonMaskRight;
      break;
    case BTN_SIDE:
      ebutton = GHOST_kButtonMaskButton4;
      break;
    case BTN_EXTRA:
      ebutton = GHOST_kButtonMaskButton5;
      break;
    case BTN_FORWARD:
      ebutton = GHOST_kButtonMaskButton6;
      break;
    case BTN_BACK:
      ebutton = GHOST_kButtonMaskButton7;
      break;
  }

  input->data_source_serial = serial;
  input->buttons.set(ebutton, state == WL_POINTER_BUTTON_STATE_PRESSED);
  input->system->pushEvent(new GHOST_EventButton(
      input->system->getMilliSeconds(), etype, win, ebutton, GHOST_TABLET_DATA_NONE));
}

static void pointer_handle_axis(void *data,
                                struct wl_pointer * /*wl_pointer*/,
                                uint32_t /*time*/,
                                uint32_t axis,
                                wl_fixed_t value)
{
  input_t *input = static_cast<input_t *>(data);
  GHOST_IWindow *win = window_from_surface(input->focus_pointer);
  if (!win) {
    return;
  }

  if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
    return;
  }

  input->system->pushEvent(
      new GHOST_EventWheel(input->system->getMilliSeconds(), win, std::signbit(value) ? +1 : -1));
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Tablet Tool), #zwp_tablet_tool_v2_listener
 * \{ */

static void tablet_tool_handle_type(void *data,
                                    struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                    uint32_t tool_type)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);

  tool_input->data.Active = tablet_tool_map_type((enum zwp_tablet_tool_v2_type)tool_type);
}

static void tablet_tool_handle_hardware_serial(void * /*data*/,
                                               struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                               uint32_t /*hardware_serial_hi*/,
                                               uint32_t /*hardware_serial_lo*/)
{
}

static void tablet_tool_handle_hardware_id_wacom(
    void * /*data*/,
    struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
    uint32_t /*hardware_id_hi*/,
    uint32_t /*hardware_id_lo*/)
{
}

static void tablet_tool_handle_capability(void * /*data*/,
                                          struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                          uint32_t /*capability*/)
{
}

static void tablet_tool_handle_done(void * /*data*/,
                                    struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/)
{
}
static void tablet_tool_handle_removed(void *data, struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;

  if (tool_input->cursor_surface) {
    wl_surface_destroy(tool_input->cursor_surface);
  }
  input->tablet_tools.erase(zwp_tablet_tool_v2);

  delete tool_input;
}
static void tablet_tool_handle_proximity_in(void *data,
                                            struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                            uint32_t serial,
                                            struct zwp_tablet_v2 * /*tablet*/,
                                            struct wl_surface *surface)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;

  input->focus_tablet = surface;
  input->tablet_serial = serial;
  input->cursor_serial = serial;

  input->data_source_serial = serial;

  /* Update #GHOST_TabletData. */
  GHOST_TabletData &td = tool_input->data;
  /* Reset, to avoid using stale tilt/pressure. */
  td.Xtilt = 0.0f;
  td.Ytilt = 0.0f;
  /* In case pressure isn't supported. */
  td.Pressure = 1.0f;

  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }
  win->activate();

  win->setCursorShape(win->getCursorShape());
}
static void tablet_tool_handle_proximity_out(void *data,
                                             struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  input->focus_tablet = nullptr;

  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }
  win->setCursorShape(win->getCursorShape());
}

static void tablet_tool_handle_down(void *data,
                                    struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                    uint32_t serial)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  const GHOST_TEventType etype = GHOST_kEventButtonDown;
  const GHOST_TButtonMask ebutton = GHOST_kButtonMaskLeft;
  input->data_source_serial = serial;
  input->buttons.set(ebutton, true);
  input->system->pushEvent(new GHOST_EventButton(
      input->system->getMilliSeconds(), etype, win, ebutton, tool_input->data));
}

static void tablet_tool_handle_up(void *data, struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  const GHOST_TEventType etype = GHOST_kEventButtonUp;
  const GHOST_TButtonMask ebutton = GHOST_kButtonMaskLeft;
  input->buttons.set(ebutton, false);
  input->system->pushEvent(new GHOST_EventButton(
      input->system->getMilliSeconds(), etype, win, ebutton, tool_input->data));
}

static void tablet_tool_handle_motion(void *data,
                                      struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                      wl_fixed_t x,
                                      wl_fixed_t y)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  input->xy[0] = x;
  input->xy[1] = y;

  const wl_fixed_t scale = win->scale();
  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 win,
                                                 wl_fixed_to_int(scale * input->xy[0]),
                                                 wl_fixed_to_int(scale * input->xy[1]),
                                                 tool_input->data));
}

static void tablet_tool_handle_pressure(void *data,
                                        struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                        uint32_t pressure)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  GHOST_TabletData &td = tool_input->data;
  td.Pressure = (float)pressure / 65535;
}
static void tablet_tool_handle_distance(void * /*data*/,
                                        struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                        uint32_t /*distance*/)
{
}
static void tablet_tool_handle_tilt(void *data,
                                    struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                    wl_fixed_t tilt_x,
                                    wl_fixed_t tilt_y)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  GHOST_TabletData &td = tool_input->data;
  /* Map degrees to `-1.0..1.0`. */
  td.Xtilt = wl_fixed_to_double(tilt_x) / 90.0f;
  td.Ytilt = wl_fixed_to_double(tilt_y) / 90.0f;
  td.Xtilt = td.Xtilt < -1.0f ? -1.0f : (td.Xtilt > 1.0f ? 1.0f : td.Xtilt);
  td.Ytilt = td.Ytilt < -1.0f ? -1.0f : (td.Ytilt > 1.0f ? 1.0f : td.Ytilt);
}

static void tablet_tool_handle_rotation(void * /*data*/,
                                        struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                        wl_fixed_t /*degrees*/)
{
  /* Pass. */
}

static void tablet_tool_handle_slider(void * /*data*/,
                                      struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                      int32_t /*position*/)
{
  /* Pass. */
}
static void tablet_tool_handle_wheel(void *data,
                                     struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                     wl_fixed_t /*degrees*/,
                                     int32_t clicks)
{
  if (clicks == 0) {
    return;
  }

  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  input->system->pushEvent(new GHOST_EventWheel(input->system->getMilliSeconds(), win, clicks));
}
static void tablet_tool_handle_button(void *data,
                                      struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                      uint32_t serial,
                                      uint32_t button,
                                      uint32_t state)
{
  tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(data);
  input_t *input = tool_input->input;
  GHOST_WindowWayland *win = window_from_surface(input->focus_tablet);
  if (!win) {
    return;
  }

  GHOST_TEventType etype = GHOST_kEventUnknown;
  switch (state) {
    case WL_POINTER_BUTTON_STATE_RELEASED:
      etype = GHOST_kEventButtonUp;
      break;
    case WL_POINTER_BUTTON_STATE_PRESSED:
      etype = GHOST_kEventButtonDown;
      break;
  }

  GHOST_TButtonMask ebutton = GHOST_kButtonMaskLeft;
  switch (button) {
    case BTN_STYLUS:
      ebutton = GHOST_kButtonMaskRight;
      break;
    case BTN_STYLUS2:
      ebutton = GHOST_kButtonMaskMiddle;
      break;
    case BTN_STYLUS3:
      ebutton = GHOST_kButtonMaskButton4;
      break;
  }

  input->data_source_serial = serial;
  input->buttons.set(ebutton, state == WL_POINTER_BUTTON_STATE_PRESSED);
  input->system->pushEvent(new GHOST_EventButton(
      input->system->getMilliSeconds(), etype, win, ebutton, tool_input->data));
}
static void tablet_tool_handle_frame(void * /*data*/,
                                     struct zwp_tablet_tool_v2 * /*zwp_tablet_tool_v2*/,
                                     uint32_t /*time*/)
{
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listner = {
    tablet_tool_handle_type,
    tablet_tool_handle_hardware_serial,
    tablet_tool_handle_hardware_id_wacom,
    tablet_tool_handle_capability,
    tablet_tool_handle_done,
    tablet_tool_handle_removed,
    tablet_tool_handle_proximity_in,
    tablet_tool_handle_proximity_out,
    tablet_tool_handle_down,
    tablet_tool_handle_up,
    tablet_tool_handle_motion,
    tablet_tool_handle_pressure,
    tablet_tool_handle_distance,
    tablet_tool_handle_tilt,
    tablet_tool_handle_rotation,
    tablet_tool_handle_slider,
    tablet_tool_handle_wheel,
    tablet_tool_handle_button,
    tablet_tool_handle_frame,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Table Seat), #zwp_tablet_seat_v2_listener
 * \{ */

static void tablet_seat_handle_tablet_added(void * /*data*/,
                                            struct zwp_tablet_seat_v2 * /*zwp_tablet_seat_v2*/,
                                            struct zwp_tablet_v2 * /*id*/)
{
  /* Pass. */
}

static void tablet_seat_handle_tool_added(void *data,
                                          struct zwp_tablet_seat_v2 * /*zwp_tablet_seat_v2*/,
                                          struct zwp_tablet_tool_v2 *id)
{
  input_t *input = static_cast<input_t *>(data);
  tablet_tool_input_t *tool_input = new tablet_tool_input_t();
  tool_input->input = input;

  /* Every tool has it's own cursor surface. */
  tool_input->cursor_surface = wl_compositor_create_surface(input->system->compositor());
  wl_surface_add_listener(tool_input->cursor_surface, &cursor_surface_listener, (void *)input);

  zwp_tablet_tool_v2_add_listener(id, &tablet_tool_listner, tool_input);

  input->tablet_tools.insert(id);
}

static void tablet_seat_handle_pad_added(void * /*data*/,
                                         struct zwp_tablet_seat_v2 * /*zwp_tablet_seat_v2*/,
                                         struct zwp_tablet_pad_v2 * /*id*/)
{
  /* Pass. */
}

const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    tablet_seat_handle_tablet_added,
    tablet_seat_handle_tool_added,
    tablet_seat_handle_pad_added,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Keyboard), #wl_keyboard_listener
 * \{ */

static void keyboard_handle_keymap(
    void *data, struct wl_keyboard * /*wl_keyboard*/, uint32_t format, int32_t fd, uint32_t size)
{
  input_t *input = static_cast<input_t *>(data);

  if ((!data) || (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)) {
    close(fd);
    return;
  }

  char *map_str = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (map_str == MAP_FAILED) {
    close(fd);
    throw std::runtime_error("keymap mmap failed: " + std::string(std::strerror(errno)));
  }

  struct xkb_keymap *keymap = xkb_keymap_new_from_string(
      input->xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_str, size);
  close(fd);

  if (!keymap) {
    return;
  }

  struct xkb_state *xkb_state_next = xkb_state_new(keymap);
  if (xkb_state_next) {
    if (input->xkb_state) {
      xkb_state_unref(input->xkb_state);
    }
    input->xkb_state = xkb_state_next;
  }
  xkb_keymap_unref(keymap);
}

/**
 * Enter event.
 *
 * Notification that this seat's keyboard focus is on a certain
 * surface.
 */
static void keyboard_handle_enter(void *data,
                                  struct wl_keyboard * /*wl_keyboard*/,
                                  uint32_t /*serial*/,
                                  struct wl_surface *surface,
                                  struct wl_array * /*keys*/)
{
  if (surface != nullptr) {
    static_cast<input_t *>(data)->focus_keyboard = surface;
  }
}

/**
 * Leave event.
 *
 * Notification that this seat's keyboard focus is no longer on a
 * certain surface.
 */
static void keyboard_handle_leave(void *data,
                                  struct wl_keyboard * /*wl_keyboard*/,
                                  uint32_t /*serial*/,
                                  struct wl_surface *surface)
{
  if (surface != nullptr) {
    static_cast<input_t *>(data)->focus_keyboard = nullptr;
  }
}

/**
 * A version of #xkb_state_key_get_one_sym which returns the key without any modifiers pressed.
 * Needed because #GHOST_TKey uses these values as key-codes.
 */
static xkb_keysym_t xkb_state_key_get_one_sym_without_modifiers(struct xkb_state *xkb_state,
                                                                xkb_keycode_t key)
{
  /* Use an empty keyboard state to access key symbol without modifiers. */
  xkb_state_get_keymap(xkb_state);
  struct xkb_keymap *keymap = xkb_state_get_keymap(xkb_state);
  struct xkb_state *xkb_state_empty = xkb_state_new(keymap);

  /* Enable number-lock. */
  {
    const xkb_mod_index_t mod2 = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
    const xkb_mod_index_t num = xkb_keymap_mod_get_index(keymap, "NumLock");
    if (num != XKB_MOD_INVALID && mod2 != XKB_MOD_INVALID) {
      xkb_state_update_mask(xkb_state_empty, (1 << mod2), 0, (1 << num), 0, 0, 0);
    }
  }

  const xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_state_empty, key);
  xkb_state_unref(xkb_state_empty);
  return sym;
}

static void keyboard_handle_key(void *data,
                                struct wl_keyboard * /*wl_keyboard*/,
                                uint32_t serial,
                                uint32_t /*time*/,
                                uint32_t key,
                                uint32_t state)
{
  input_t *input = static_cast<input_t *>(data);

  GHOST_TEventType etype = GHOST_kEventUnknown;
  switch (state) {
    case WL_KEYBOARD_KEY_STATE_RELEASED:
      etype = GHOST_kEventKeyUp;
      break;
    case WL_KEYBOARD_KEY_STATE_PRESSED:
      etype = GHOST_kEventKeyDown;
      break;
  }

  const xkb_keysym_t sym = xkb_state_key_get_one_sym_without_modifiers(input->xkb_state, key + 8);

  if (sym == XKB_KEY_NoSymbol) {
    return;
  }

  /* Delete previous timer. */
  if (xkb_keymap_key_repeats(xkb_state_get_keymap(input->xkb_state), key + 8) &&
      input->key_repeat.timer) {
    delete static_cast<key_repeat_payload_t *>(input->key_repeat.timer->getUserData());
    input->system->removeTimer(input->key_repeat.timer);
    input->key_repeat.timer = nullptr;
  }

  GHOST_TEventKeyData key_data = {
      .key = xkb_map_gkey(sym),
  };

  if (etype == GHOST_kEventKeyDown) {
    xkb_state_key_get_utf8(
        input->xkb_state, key + 8, key_data.utf8_buf, sizeof(GHOST_TEventKeyData::utf8_buf));
  }
  else {
    key_data.utf8_buf[0] = '\0';
  }

  input->data_source_serial = serial;

  GHOST_IWindow *win = static_cast<GHOST_WindowWayland *>(
      wl_surface_get_user_data(input->focus_keyboard));
  input->system->pushEvent(new GHOST_EventKey(
      input->system->getMilliSeconds(), etype, win, key_data.key, '\0', key_data.utf8_buf, false));

  /* Start timer for repeating key, if applicable. */
  if (input->key_repeat.rate > 0 &&
      xkb_keymap_key_repeats(xkb_state_get_keymap(input->xkb_state), key + 8) &&
      etype == GHOST_kEventKeyDown) {

    key_repeat_payload_t *payload = new key_repeat_payload_t({
        .system = input->system,
        .window = win,
        .key_data = key_data,
    });

    auto key_repeat_fn = [](GHOST_ITimerTask *task, uint64_t /*time*/) {
      struct key_repeat_payload_t *payload = static_cast<key_repeat_payload_t *>(
          task->getUserData());
      payload->system->pushEvent(new GHOST_EventKey(payload->system->getMilliSeconds(),
                                                    GHOST_kEventKeyDown,
                                                    payload->window,
                                                    payload->key_data.key,
                                                    '\0',
                                                    payload->key_data.utf8_buf,
                                                    true));
    };
    input->key_repeat.timer = input->system->installTimer(
        input->key_repeat.delay, 1000 / input->key_repeat.rate, key_repeat_fn, payload);
  }
}

static void keyboard_handle_modifiers(void *data,
                                      struct wl_keyboard * /*wl_keyboard*/,
                                      uint32_t /*serial*/,
                                      uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked,
                                      uint32_t group)
{
  xkb_state_update_mask(static_cast<input_t *>(data)->xkb_state,
                        mods_depressed,
                        mods_latched,
                        mods_locked,
                        0,
                        0,
                        group);
}

static void keyboard_repeat_handle_info(void *data,
                                        struct wl_keyboard * /*wl_keyboard*/,
                                        int32_t rate,
                                        int32_t delay)
{
  input_t *input = static_cast<input_t *>(data);

  input->key_repeat.rate = rate;
  input->key_repeat.delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_repeat_handle_info,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Seat), #wl_seat_listener
 * \{ */

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
  input_t *input = static_cast<input_t *>(data);
  input->wl_pointer = nullptr;
  input->wl_keyboard = nullptr;

  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    input->wl_pointer = wl_seat_get_pointer(wl_seat);
    input->cursor.wl_surface = wl_compositor_create_surface(input->system->compositor());
    input->cursor.visible = true;
    input->cursor.wl_buffer = nullptr;
    input->cursor.file_buffer = new buffer_t;
    if (!get_cursor_settings(input->cursor.theme_name, input->cursor.size)) {
      input->cursor.theme_name = std::string();
      input->cursor.size = default_cursor_size;
    }
    wl_pointer_add_listener(input->wl_pointer, &pointer_listener, data);
    wl_surface_add_listener(input->cursor.wl_surface, &cursor_surface_listener, data);
  }

  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    input->wl_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(input->wl_keyboard, &keyboard_listener, data);
  }
}

static void seat_handle_name(void *data, struct wl_seat * /*wl_seat*/, const char *name)
{
  static_cast<input_t *>(data)->name = std::string(name);
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG Output), #zxdg_output_v1_listener
 * \{ */

static void xdg_output_handle_logical_position(void *data,
                                               struct zxdg_output_v1 * /*xdg_output*/,
                                               int32_t x,
                                               int32_t y)
{
  output_t *output = static_cast<output_t *>(data);
  output->position_logical[0] = x;
  output->position_logical[1] = y;
  output->has_position_logical = true;
}

static void xdg_output_handle_logical_size(void *data,
                                           struct zxdg_output_v1 * /*xdg_output*/,
                                           int32_t width,
                                           int32_t height)
{
  output_t *output = static_cast<output_t *>(data);

  if (output->size_logical[0] != 0 && output->size_logical[1] != 0) {
    /* Original comment from SDL. */
    /* FIXME(@flibit): GNOME has a bug where the logical size does not account for
     * scale, resulting in bogus viewport sizes.
     *
     * Until this is fixed, validate that _some_ kind of scaling is being
     * done (we can't match exactly because fractional scaling can't be
     * detected otherwise), then override if necessary. */
    if ((output->size_logical[0] == width) && (output->scale_fractional == wl_fixed_from_int(1))) {
      GHOST_PRINT("xdg_output scale did not match, overriding with wl_output scale");
      return;
    }
  }

  output->size_logical[0] = width;
  output->size_logical[1] = height;
  output->has_size_logical = true;
}

static void xdg_output_handle_done(void * /*data*/, struct zxdg_output_v1 * /*xdg_output*/)
{
  /* NOTE: `xdg-output.done` events are deprecated and only apply below version 3 of the protocol.
   * `wl-output.done` event will be emitted in version 3 or higher. */
}

static void xdg_output_handle_name(void * /*data*/,
                                   struct zxdg_output_v1 * /*xdg_output*/,
                                   const char * /*name*/)
{
  /* Pass. */
}

static void xdg_output_handle_description(void * /*data*/,
                                          struct zxdg_output_v1 * /*xdg_output*/,
                                          const char * /*description*/)
{
  /* Pass. */
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    xdg_output_handle_logical_position,
    xdg_output_handle_logical_size,
    xdg_output_handle_done,
    xdg_output_handle_name,
    xdg_output_handle_description,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Output), #wl_output_listener
 * \{ */

static void output_handle_geometry(void *data,
                                   struct wl_output * /*wl_output*/,
                                   int32_t /*x*/,
                                   int32_t /*y*/,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t /*subpixel*/,
                                   const char *make,
                                   const char *model,
                                   int32_t transform)
{
  output_t *output = static_cast<output_t *>(data);
  output->transform = transform;
  output->make = std::string(make);
  output->model = std::string(model);
  output->size_mm[0] = physical_width;
  output->size_mm[1] = physical_height;
}

static void output_handle_mode(void *data,
                               struct wl_output * /*wl_output*/,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t /*refresh*/)
{
  output_t *output = static_cast<output_t *>(data);

  if (flags & WL_OUTPUT_MODE_CURRENT) {
    output->size_native[0] = width;
    output->size_native[1] = height;

    /* Don't rotate this yet, `wl-output` coordinates are transformed in
     * handle_done and `xdg-output` coordinates are pre-transformed. */
    if (!output->has_size_logical) {
      output->size_logical[0] = width;
      output->size_logical[1] = height;
    }
  }
}

/**
 * Sent all information about output.
 *
 * This event is sent after all other properties have been sent
 * after binding to the output object and after any other property
 * changes done after that. This allows changes to the output
 * properties to be seen as atomic, even if they happen via multiple events.
 */
static void output_handle_done(void *data, struct wl_output * /*wl_output*/)
{
  output_t *output = static_cast<output_t *>(data);
  int32_t size_native[2];
  if (output->transform & WL_OUTPUT_TRANSFORM_90) {
    size_native[0] = output->size_native[1];
    size_native[1] = output->size_native[1];
  }
  else {
    size_native[0] = output->size_native[0];
    size_native[1] = output->size_native[1];
  }

  /* If `xdg-output` is present, calculate the true scale of the desktop */
  if (output->has_size_logical) {

    /* NOTE: it's not necessary to divide these values by their greatest-common-denominator
     * as even a 64k screen resolution doesn't approach overflowing an `int32_t`. */

    GHOST_ASSERT(size_native[0] && output->size_logical[0],
                 "Screen size values were not set when they were expected to be.");

    output->scale_fractional = wl_fixed_from_int(size_native[0]) / output->size_logical[0];
    output->has_scale_fractional = true;
  }
}

static void output_handle_scale(void *data, struct wl_output * /*wl_output*/, int32_t factor)
{
  static_cast<output_t *>(data)->scale = factor;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG WM Base), #xdg_wm_base_listener
 * \{ */

static void shell_handle_ping(void * /*data*/, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener shell_listener = {
    shell_handle_ping,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Registry), #wl_registry_listener
 * \{ */

static void global_handle_add(void *data,
                              struct wl_registry *wl_registry,
                              uint32_t name,
                              const char *interface,
                              uint32_t /*version*/)
{
  struct display_t *display = static_cast<struct display_t *>(data);
  if (!strcmp(interface, wl_compositor_interface.name)) {
    display->compositor = static_cast<wl_compositor *>(
        wl_registry_bind(wl_registry, name, &wl_compositor_interface, 3));
  }
  else if (!strcmp(interface, xdg_wm_base_interface.name)) {
    display->xdg_shell = static_cast<xdg_wm_base *>(
        wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(display->xdg_shell, &shell_listener, nullptr);
  }
  else if (!strcmp(interface, zxdg_decoration_manager_v1_interface.name)) {
    display->xdg_decoration_manager = static_cast<zxdg_decoration_manager_v1 *>(
        wl_registry_bind(wl_registry, name, &zxdg_decoration_manager_v1_interface, 1));
  }
  else if (!strcmp(interface, zxdg_output_manager_v1_interface.name)) {
    display->xdg_output_manager = static_cast<zxdg_output_manager_v1 *>(
        wl_registry_bind(wl_registry, name, &zxdg_output_manager_v1_interface, 3));
    for (output_t *output : display->outputs) {
      output->xdg_output = zxdg_output_manager_v1_get_xdg_output(display->xdg_output_manager,
                                                                 output->wl_output);
      zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output);
    }
  }
  else if (!strcmp(interface, wl_output_interface.name)) {
    output_t *output = new output_t;
    output->wl_output = static_cast<wl_output *>(
        wl_registry_bind(wl_registry, name, &wl_output_interface, 2));
    display->outputs.push_back(output);
    wl_output_add_listener(output->wl_output, &output_listener, output);

    if (display->xdg_output_manager) {
      output->xdg_output = zxdg_output_manager_v1_get_xdg_output(display->xdg_output_manager,
                                                                 output->wl_output);
      zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output);
    }
  }
  else if (!strcmp(interface, wl_seat_interface.name)) {
    input_t *input = new input_t;
    input->system = display->system;
    input->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    input->data_source = new data_source_t;
    input->wl_seat = static_cast<wl_seat *>(
        wl_registry_bind(wl_registry, name, &wl_seat_interface, 4));
    display->inputs.push_back(input);
    wl_seat_add_listener(input->wl_seat, &seat_listener, input);
  }
  else if (!strcmp(interface, wl_shm_interface.name)) {
    display->shm = static_cast<wl_shm *>(
        wl_registry_bind(wl_registry, name, &wl_shm_interface, 1));
  }
  else if (!strcmp(interface, wl_data_device_manager_interface.name)) {
    display->data_device_manager = static_cast<wl_data_device_manager *>(
        wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 3));
  }
  else if (!strcmp(interface, zwp_tablet_manager_v2_interface.name)) {
    display->tablet_manager = static_cast<zwp_tablet_manager_v2 *>(
        wl_registry_bind(wl_registry, name, &zwp_tablet_manager_v2_interface, 1));
  }
  else if (!strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)) {
    display->relative_pointer_manager = static_cast<zwp_relative_pointer_manager_v1 *>(
        wl_registry_bind(wl_registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
  }
  else if (!strcmp(interface, zwp_pointer_constraints_v1_interface.name)) {
    display->pointer_constraints = static_cast<zwp_pointer_constraints_v1 *>(
        wl_registry_bind(wl_registry, name, &zwp_pointer_constraints_v1_interface, 1));
  }
}

/**
 * Announce removal of global object.
 *
 * Notify the client of removed global objects.
 *
 * This event notifies the client that the global identified by
 * name is no longer available. If the client bound to the global
 * using the bind request, the client should now destroy that object.
 */
static void global_handle_remove(void * /*data*/,
                                 struct wl_registry * /*wl_registry*/,
                                 uint32_t /*name*/)
{
}

static const struct wl_registry_listener registry_listener = {
    global_handle_add,
    global_handle_remove,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ghost Implementation
 *
 * Wayland specific implementation of the GHOST_System interface.
 * \{ */

GHOST_SystemWayland::GHOST_SystemWayland() : GHOST_System(), d(new display_t)
{
  wl_log_set_handler_client(ghost_wayland_log_handler);

  d->system = this;
  /* Connect to the Wayland server. */
  d->display = wl_display_connect(nullptr);
  if (!d->display) {
    display_destroy(d);
    throw std::runtime_error("Wayland: unable to connect to display!");
  }

  /* Register interfaces. */
  struct wl_registry *registry = wl_display_get_registry(d->display);
  wl_registry_add_listener(registry, &registry_listener, d);
  /* Call callback for registry listener. */
  wl_display_roundtrip(d->display);
  /* Call callbacks for registered listeners. */
  wl_display_roundtrip(d->display);
  wl_registry_destroy(registry);

  if (!d->xdg_shell) {
    display_destroy(d);
    throw std::runtime_error("Wayland: unable to access xdg_shell!");
  }

  /* Register data device per seat for IPC between Wayland clients. */
  if (d->data_device_manager) {
    for (input_t *input : d->inputs) {
      input->data_device = wl_data_device_manager_get_data_device(d->data_device_manager,
                                                                  input->wl_seat);
      wl_data_device_add_listener(input->data_device, &data_device_listener, input);
    }
  }

  if (d->tablet_manager) {
    for (input_t *input : d->inputs) {
      input->tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(d->tablet_manager,
                                                                 input->wl_seat);
      zwp_tablet_seat_v2_add_listener(input->tablet_seat, &tablet_seat_listener, input);
    }
  }
}

GHOST_SystemWayland::~GHOST_SystemWayland()
{
  display_destroy(d);
}

bool GHOST_SystemWayland::processEvents(bool waitForEvent)
{
  const bool fired = getTimerManager()->fireTimers(getMilliSeconds());

  if (waitForEvent) {
    wl_display_dispatch(d->display);
  }
  else {
    wl_display_roundtrip(d->display);
  }

  return fired || (getEventManager()->getNumEvents() > 0);
}

int GHOST_SystemWayland::setConsoleWindowState(GHOST_TConsoleWindowState /*action*/)
{
  return 0;
}

GHOST_TSuccess GHOST_SystemWayland::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  static const xkb_state_component mods_all = xkb_state_component(
      XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED | XKB_STATE_MODS_LOCKED |
      XKB_STATE_MODS_EFFECTIVE);

  bool val;

  /* NOTE: XKB doesn't seem to differentiate between left/right modifiers. */

  val = xkb_state_mod_name_is_active(d->inputs[0]->xkb_state, XKB_MOD_NAME_SHIFT, mods_all) == 1;
  keys.set(GHOST_kModifierKeyLeftShift, val);
  keys.set(GHOST_kModifierKeyRightShift, val);

  val = xkb_state_mod_name_is_active(d->inputs[0]->xkb_state, XKB_MOD_NAME_ALT, mods_all) == 1;
  keys.set(GHOST_kModifierKeyLeftAlt, val);
  keys.set(GHOST_kModifierKeyRightAlt, val);

  val = xkb_state_mod_name_is_active(d->inputs[0]->xkb_state, XKB_MOD_NAME_CTRL, mods_all) == 1;
  keys.set(GHOST_kModifierKeyLeftControl, val);
  keys.set(GHOST_kModifierKeyRightControl, val);

  val = xkb_state_mod_name_is_active(d->inputs[0]->xkb_state, XKB_MOD_NAME_LOGO, mods_all) == 1;
  keys.set(GHOST_kModifierKeyOS, val);

  val = xkb_state_mod_name_is_active(d->inputs[0]->xkb_state, XKB_MOD_NAME_NUM, mods_all) == 1;
  keys.set(GHOST_kModifierKeyNumMasks, val);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWayland::getButtons(GHOST_Buttons &buttons) const
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  buttons = d->inputs[0]->buttons;
  return GHOST_kSuccess;
}

char *GHOST_SystemWayland::getClipboard(bool /*selection*/) const
{
  char *clipboard = static_cast<char *>(malloc((selection.size() + 1)));
  memcpy(clipboard, selection.data(), selection.size() + 1);
  return clipboard;
}

void GHOST_SystemWayland::putClipboard(const char *buffer, bool /*selection*/) const
{
  if (!d->data_device_manager || d->inputs.empty()) {
    return;
  }

  input_t *input = d->inputs[0];

  std::lock_guard lock{input->data_source_mutex};

  data_source_t *data_source = input->data_source;

  /* Copy buffer. */
  free(data_source->buffer_out);
  const size_t buffer_size = strlen(buffer) + 1;
  data_source->buffer_out = static_cast<char *>(malloc(buffer_size));
  std::memcpy(data_source->buffer_out, buffer, buffer_size);

  data_source->data_source = wl_data_device_manager_create_data_source(d->data_device_manager);

  wl_data_source_add_listener(data_source->data_source, &data_source_listener, input);

  for (const std::string &type : mime_send) {
    wl_data_source_offer(data_source->data_source, type.c_str());
  }

  if (input->data_device) {
    wl_data_device_set_selection(
        input->data_device, data_source->data_source, input->data_source_serial);
  }
}

uint8_t GHOST_SystemWayland::getNumDisplays() const
{
  return d ? uint8_t(d->outputs.size()) : 0;
}

GHOST_TSuccess GHOST_SystemWayland::getCursorPosition(int32_t &x, int32_t &y) const
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  input_t *input = d->inputs[0];
  struct wl_surface *surface = nullptr;
  if (input->pointer_serial == input->cursor_serial) {
    surface = input->focus_pointer;
  }
  else if (input->tablet_serial == input->cursor_serial) {
    surface = input->focus_tablet;
  }
  if (!surface) {
    return GHOST_kFailure;
  }

  GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(wl_surface_get_user_data(surface));
  if (!win) {
    return GHOST_kFailure;
  }

  const wl_fixed_t scale = win->scale();
  x = wl_fixed_to_int(scale * input->xy[0]);
  y = wl_fixed_to_int(scale * input->xy[1]);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWayland::setCursorPosition(int32_t /*x*/, int32_t /*y*/)
{
  return GHOST_kFailure;
}

void GHOST_SystemWayland::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  if (getNumDisplays() > 0) {
    /* We assume first output as main. */
    width = uint32_t(d->outputs[0]->size_native[0]) / d->outputs[0]->scale;
    height = uint32_t(d->outputs[0]->size_native[1]) / d->outputs[0]->scale;
  }
}

void GHOST_SystemWayland::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  getMainDisplayDimensions(width, height);
}

GHOST_IContext *GHOST_SystemWayland::createOffscreenContext(GHOST_GLSettings /*glSettings*/)
{
  /* Create new off-screen window. */
  wl_surface *os_surface = wl_compositor_create_surface(compositor());
  wl_egl_window *os_egl_window = wl_egl_window_create(os_surface, int(1), int(1));

  d->os_surfaces.push_back(os_surface);
  d->os_egl_windows.push_back(os_egl_window);

  GHOST_Context *context;

  for (int minor = 6; minor >= 0; --minor) {
    context = new GHOST_ContextEGL(this,
                                   false,
                                   EGLNativeWindowType(os_egl_window),
                                   EGLNativeDisplayType(d->display),
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

  context = new GHOST_ContextEGL(this,
                                 false,
                                 EGLNativeWindowType(os_egl_window),
                                 EGLNativeDisplayType(d->display),
                                 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                 3,
                                 3,
                                 GHOST_OPENGL_EGL_CONTEXT_FLAGS,
                                 GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
                                 EGL_OPENGL_API);

  if (context->initializeDrawingContext()) {
    return context;
  }
  delete context;

  GHOST_PRINT("Cannot create off-screen EGL context" << std::endl);

  return nullptr;
}

GHOST_TSuccess GHOST_SystemWayland::disposeContext(GHOST_IContext *context)
{
  delete context;
  return GHOST_kSuccess;
}

GHOST_IWindow *GHOST_SystemWayland::createWindow(const char *title,
                                                 int32_t left,
                                                 int32_t top,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 GHOST_TWindowState state,
                                                 GHOST_TDrawingContextType type,
                                                 GHOST_GLSettings glSettings,
                                                 const bool exclusive,
                                                 const bool is_dialog,
                                                 const GHOST_IWindow *parentWindow)
{
  /* globally store pointer to window manager */
  if (!window_manager) {
    window_manager = getWindowManager();
  }

  GHOST_WindowWayland *window = new GHOST_WindowWayland(
      this,
      title,
      left,
      top,
      width,
      height,
      state,
      parentWindow,
      type,
      is_dialog,
      ((glSettings.flags & GHOST_glStereoVisual) != 0),
      exclusive);

  if (window) {
    if (window->getValid()) {
      m_windowManager->addWindow(window);
      m_windowManager->setActiveWindow(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      delete window;
      window = nullptr;
    }
  }

  return window;
}

wl_display *GHOST_SystemWayland::display()
{
  return d->display;
}

wl_compositor *GHOST_SystemWayland::compositor()
{
  return d->compositor;
}

xdg_wm_base *GHOST_SystemWayland::xdg_shell()
{
  return d->xdg_shell;
}

zxdg_decoration_manager_v1 *GHOST_SystemWayland::xdg_decoration_manager()
{
  return d->xdg_decoration_manager;
}

const std::vector<output_t *> &GHOST_SystemWayland::outputs() const
{
  return d->outputs;
}

wl_shm *GHOST_SystemWayland::shm() const
{
  return d->shm;
}

void GHOST_SystemWayland::setSelection(const std::string &selection)
{
  this->selection = selection;
}

static void set_cursor_buffer(input_t *input, wl_buffer *buffer)
{
  cursor_t *c = &input->cursor;

  c->visible = (buffer != nullptr);

  const int32_t image_size_x = int32_t(c->wl_image.width);
  const int32_t image_size_y = int32_t(c->wl_image.height);

  const int32_t hotspot_x = int32_t(c->wl_image.hotspot_x) / c->scale;
  const int32_t hotspot_y = int32_t(c->wl_image.hotspot_y) / c->scale;

  if (buffer) {
    wl_surface_set_buffer_scale(c->wl_surface, c->scale);
    wl_surface_attach(c->wl_surface, buffer, 0, 0);
    wl_surface_damage(c->wl_surface, 0, 0, image_size_x, image_size_y);
    wl_surface_commit(c->wl_surface);
  }

  wl_pointer_set_cursor(input->wl_pointer,
                        input->pointer_serial,
                        c->visible ? c->wl_surface : nullptr,
                        hotspot_x,
                        hotspot_y);

  /* Set the cursor for all tablet tools as well. */
  for (struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2 : input->tablet_tools) {
    tablet_tool_input_t *tool_input = static_cast<tablet_tool_input_t *>(
        zwp_tablet_tool_v2_get_user_data(zwp_tablet_tool_v2));

    if (buffer) {
      /* FIXME: for some reason cursor scale is applied twice (when the scale isn't 1x),
       * this happens both in gnome-shell & KDE. Setting the surface scale here doesn't help. */
      wl_surface_set_buffer_scale(tool_input->cursor_surface, c->scale);
      wl_surface_attach(tool_input->cursor_surface, buffer, 0, 0);
      wl_surface_damage(tool_input->cursor_surface, 0, 0, image_size_x, image_size_y);
      wl_surface_commit(tool_input->cursor_surface);
    }

    zwp_tablet_tool_v2_set_cursor(zwp_tablet_tool_v2,
                                  input->tablet_serial,
                                  c->visible ? tool_input->cursor_surface : nullptr,
                                  hotspot_x,
                                  hotspot_y);
  }
}

GHOST_TSuccess GHOST_SystemWayland::setCursorShape(GHOST_TStandardCursor shape)
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }
  const std::string cursor_name = cursors.count(shape) ? cursors.at(shape) :
                                                         cursors.at(GHOST_kStandardCursorDefault);

  input_t *input = d->inputs[0];
  cursor_t *c = &input->cursor;

  if (!c->wl_theme) {
    /* The cursor surface hasn't entered an output yet. Initialize theme with scale 1. */
    c->wl_theme = wl_cursor_theme_load(
        c->theme_name.c_str(), c->size, d->inputs[0]->system->shm());
  }

  wl_cursor *cursor = wl_cursor_theme_get_cursor(c->wl_theme, cursor_name.c_str());

  if (!cursor) {
    GHOST_PRINT("cursor '" << cursor_name << "' does not exist" << std::endl);
    return GHOST_kFailure;
  }

  struct wl_cursor_image *image = cursor->images[0];
  struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
  if (!buffer) {
    return GHOST_kFailure;
  }

  c->wl_buffer = buffer;
  c->wl_image = *image;

  set_cursor_buffer(input, buffer);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWayland::hasCursorShape(GHOST_TStandardCursor cursorShape)
{
  return GHOST_TSuccess(cursors.count(cursorShape) && !cursors.at(cursorShape).empty());
}

GHOST_TSuccess GHOST_SystemWayland::setCustomCursorShape(uint8_t *bitmap,
                                                         uint8_t *mask,
                                                         int sizex,
                                                         int sizey,
                                                         int hotX,
                                                         int hotY,
                                                         bool /*canInvertColor*/)
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  cursor_t *cursor = &d->inputs[0]->cursor;

  static const int32_t stride = sizex * 4; /* ARGB */
  cursor->file_buffer->size = (size_t)stride * sizey;

#ifdef HAVE_MEMFD_CREATE
  const int fd = memfd_create("blender-cursor-custom", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd >= 0) {
    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
  }
#else
  char *path = getenv("XDG_RUNTIME_DIR");
  if (!path) {
    errno = ENOENT;
    return GHOST_kFailure;
  }

  char *tmpname;
  asprintf(&tmpname, "%s/%s", path, "blender-XXXXXX");
  const int fd = mkostemp(tmpname, O_CLOEXEC);
  if (fd >= 0) {
    unlink(tmpname);
  }
  free(tmpname);
#endif

  if (fd < 0) {
    return GHOST_kFailure;
  }

  if (posix_fallocate(fd, 0, int32_t(cursor->file_buffer->size)) != 0) {
    return GHOST_kFailure;
  }

  cursor->file_buffer->data = mmap(
      nullptr, cursor->file_buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (cursor->file_buffer->data == MAP_FAILED) {
    close(fd);
    return GHOST_kFailure;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(d->shm, fd, int32_t(cursor->file_buffer->size));

  wl_buffer *buffer = wl_shm_pool_create_buffer(
      pool, 0, sizex, sizey, stride, WL_SHM_FORMAT_ARGB8888);

  wl_shm_pool_destroy(pool);
  close(fd);

  wl_buffer_add_listener(buffer, &cursor_buffer_listener, cursor);

  static constexpr uint32_t black = 0xFF000000;
  static constexpr uint32_t white = 0xFFFFFFFF;
  static constexpr uint32_t transparent = 0x00000000;

  uint8_t datab = 0, maskb = 0;
  uint32_t *pixel;

  for (int y = 0; y < sizey; ++y) {
    pixel = &static_cast<uint32_t *>(cursor->file_buffer->data)[y * sizex];
    for (int x = 0; x < sizex; ++x) {
      if ((x % 8) == 0) {
        datab = *bitmap++;
        maskb = *mask++;

        /* Reverse bit order. */
        datab = uint8_t((datab * 0x0202020202ULL & 0x010884422010ULL) % 1023);
        maskb = uint8_t((maskb * 0x0202020202ULL & 0x010884422010ULL) % 1023);
      }

      if (maskb & 0x80) {
        *pixel++ = (datab & 0x80) ? white : black;
      }
      else {
        *pixel++ = (datab & 0x80) ? white : transparent;
      }
      datab <<= 1;
      maskb <<= 1;
    }
  }

  cursor->wl_buffer = buffer;
  cursor->wl_image.width = uint32_t(sizex);
  cursor->wl_image.height = uint32_t(sizey);
  cursor->wl_image.hotspot_x = uint32_t(hotX);
  cursor->wl_image.hotspot_y = uint32_t(hotY);

  set_cursor_buffer(d->inputs[0], buffer);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWayland::setCursorVisibility(bool visible)
{
  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  input_t *input = d->inputs[0];

  cursor_t *cursor = &input->cursor;
  if (visible) {
    if (!cursor->visible) {
      set_cursor_buffer(input, cursor->wl_buffer);
    }
  }
  else {
    if (cursor->visible) {
      set_cursor_buffer(input, nullptr);
    }
  }

  return GHOST_kSuccess;
}

bool GHOST_SystemWayland::supportsCursorWarp()
{
  /* WAYLAND doesn't support setting the cursor position directly,
   * this is an intentional choice, forcing us to use a software cursor in this case. */
  return false;
}

bool GHOST_SystemWayland::supportsWindowPosition()
{
  /* WAYLAND doesn't support accessing the window position. */
  return false;
}

GHOST_TSuccess GHOST_SystemWayland::setCursorGrab(const GHOST_TGrabCursorMode mode,
                                                  const GHOST_TGrabCursorMode mode_current,
                                                  wl_surface *surface)
{
  /* ignore, if the required protocols are not supported */
  if (!d->relative_pointer_manager || !d->pointer_constraints) {
    return GHOST_kFailure;
  }

  if (d->inputs.empty()) {
    return GHOST_kFailure;
  }

  /* No change, success. */
  if (mode == mode_current) {
    return GHOST_kSuccess;
  }

  input_t *input = d->inputs[0];

#define MODE_NEEDS_LOCK(m) ((m) == GHOST_kGrabWrap || (m) == GHOST_kGrabHide)
#define MODE_NEEDS_HIDE(m) ((m) == GHOST_kGrabHide)
#define MODE_NEEDS_CONFINE(m) ((m) == GHOST_kGrabNormal)

  const bool was_lock = MODE_NEEDS_LOCK(mode_current);
  const bool use_lock = MODE_NEEDS_LOCK(mode);

  /* Check for wrap as #supportsCursorWarp isn't supported. */
  const bool was_hide = MODE_NEEDS_HIDE(mode_current) || (mode_current == GHOST_kGrabWrap);
  const bool use_hide = MODE_NEEDS_HIDE(mode) || (mode == GHOST_kGrabWrap);

  const bool was_confine = MODE_NEEDS_CONFINE(mode_current);
  const bool use_confine = MODE_NEEDS_CONFINE(mode);

#undef MODE_NEEDS_LOCK
#undef MODE_NEEDS_HIDE
#undef MODE_NEEDS_CONFINE

  if (!use_hide) {
    setCursorVisibility(true);
  }

  /* Switching from one grab mode to another,
   * in this case disable the current locks as it makes logic confusing,
   * postpone changing the cursor to avoid flickering. */
  if (!use_lock) {
    if (input->relative_pointer) {
      zwp_relative_pointer_v1_destroy(input->relative_pointer);
      input->relative_pointer = nullptr;
    }
    if (input->locked_pointer) {
      /* Request location to restore to. */
      if (mode_current == GHOST_kGrabWrap) {
        /* The chance this fails is _very_ low. */
        GHOST_WindowWayland *win = window_from_surface(surface);
        if (!win) {
          GHOST_PRINT("could not find window from surface when un-grabbing!" << std::endl);
        }
        else {
          GHOST_Rect bounds;
          int32_t xy_new[2] = {input->xy[0], input->xy[1]};

          /* Fallback to window bounds. */
          if (win->getCursorGrabBounds(bounds) == GHOST_kFailure) {
            win->getClientBounds(bounds);
          }

          const int scale = win->scale();

          bounds.m_l = wl_fixed_from_int(bounds.m_l) / scale;
          bounds.m_t = wl_fixed_from_int(bounds.m_t) / scale;
          bounds.m_r = wl_fixed_from_int(bounds.m_r) / scale;
          bounds.m_b = wl_fixed_from_int(bounds.m_b) / scale;

          bounds.wrapPoint(xy_new[0], xy_new[1], 0, win->getCursorGrabAxis());

          /* Push an event so the new location is registered. */
          if ((xy_new[0] != input->xy[0]) || (xy_new[1] != input->xy[1])) {
            input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                           GHOST_kEventCursorMove,
                                                           win,
                                                           wl_fixed_to_int(scale * xy_new[0]),
                                                           wl_fixed_to_int(scale * xy_new[1]),
                                                           GHOST_TABLET_DATA_NONE));
          }
          input->xy[0] = xy_new[0];
          input->xy[1] = xy_new[1];

          zwp_locked_pointer_v1_set_cursor_position_hint(
              input->locked_pointer, xy_new[0], xy_new[1]);
          wl_surface_commit(surface);
        }
      }

      zwp_locked_pointer_v1_destroy(input->locked_pointer);
      input->locked_pointer = nullptr;
    }
  }

  if (!use_confine) {
    if (input->confined_pointer) {
      zwp_confined_pointer_v1_destroy(input->confined_pointer);
      input->confined_pointer = nullptr;
    }
  }

  if (mode != GHOST_kGrabDisable) {
    if (use_lock) {
      if (!was_lock) {
        /* TODO(@campbellbarton): As WAYLAND does not support warping the pointer it may not be
         * possible to support #GHOST_kGrabWrap by pragmatically settings it's coordinates.
         * An alternative could be to draw the cursor in software (and hide the real cursor),
         * or just accept a locked cursor on WAYLAND. */
        input->relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
            d->relative_pointer_manager, input->wl_pointer);
        zwp_relative_pointer_v1_add_listener(
            input->relative_pointer, &relative_pointer_listener, input);
        input->locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
            d->pointer_constraints,
            surface,
            input->wl_pointer,
            nullptr,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
      }
    }
    else if (use_confine) {
      if (!was_confine) {
        input->confined_pointer = zwp_pointer_constraints_v1_confine_pointer(
            d->pointer_constraints,
            surface,
            input->wl_pointer,
            nullptr,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
      }
    }

    if (use_hide && !was_hide) {
      setCursorVisibility(false);
    }
  }

  return GHOST_kSuccess;
}

/** \} */
