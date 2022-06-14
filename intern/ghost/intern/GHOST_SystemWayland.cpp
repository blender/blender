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
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <mutex>

static GHOST_IWindow *get_window(struct wl_surface *surface);

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

struct buffer_t {
  void *data;
  size_t size;
};

struct cursor_t {
  bool visible;
  struct wl_surface *surface = nullptr;
  struct wl_buffer *buffer;
  struct wl_cursor_image image;
  struct buffer_t *file_buffer = nullptr;
  struct wl_cursor_theme *theme = nullptr;
  int size;
  std::string theme_name;
  /** Outputs on which the cursor is visible. */
  std::unordered_set<const output_t *> outputs;
  int scale = 1;
};

struct data_offer_t {
  std::unordered_set<std::string> types;
  uint32_t source_actions;
  uint32_t dnd_action;
  struct wl_data_offer *id;
  std::atomic<bool> in_use;
  struct {
    int x, y;
  } dnd;
};

struct data_source_t {
  struct wl_data_source *data_source;
  char *buffer_out;
};

struct key_repeat_payload_t {
  GHOST_SystemWayland *system;
  GHOST_IWindow *window;
  GHOST_TKey key;
  GHOST_TEventKeyData key_data;
};

struct input_t {
  GHOST_SystemWayland *system;

  std::string name;
  struct wl_seat *seat;
  struct wl_pointer *pointer = nullptr;
  struct wl_keyboard *keyboard = nullptr;

  uint32_t pointer_serial;
  int x, y;
  GHOST_Buttons buttons;
  struct cursor_t cursor;

  struct zwp_relative_pointer_v1 *relative_pointer;
  struct zwp_locked_pointer_v1 *locked_pointer;
  struct zwp_confined_pointer_v1 *confined_pointer;

  struct xkb_context *xkb_context;
  struct xkb_state *xkb_state;
  struct {
    /** Key repetition in character per second. */
    int32_t rate;
    /** Time (milliseconds) after which to start repeating keys. */
    int32_t delay;
    /** Timer for key repeats. */
    GHOST_ITimerTask *timer = nullptr;
  } key_repeat;

  struct wl_surface *focus_pointer = nullptr;
  struct wl_surface *focus_keyboard = nullptr;
  struct wl_surface *focus_dnd = nullptr;

  struct wl_data_device *data_device = nullptr;
  /** Drag & Drop. */
  struct data_offer_t *data_offer_dnd;
  std::mutex data_offer_dnd_mutex;

  /** Copy & Paste. */
  struct data_offer_t *data_offer_copy_paste;
  std::mutex data_offer_copy_paste_mutex;

  struct data_source_t *data_source;
  std::mutex data_source_mutex;

  /** Last device that was active. */
  uint32_t data_source_serial;
};

struct display_t {
  GHOST_SystemWayland *system;

  struct wl_display *display;
  struct wl_compositor *compositor = nullptr;
  struct xdg_wm_base *xdg_shell = nullptr;
  struct zxdg_decoration_manager_v1 *xdg_decoration_manager = nullptr;
  struct wl_shm *shm = nullptr;
  std::vector<output_t *> outputs;
  std::vector<input_t *> inputs;
  struct {
    std::string theme;
    int size;
  } cursor;
  struct wl_data_device_manager *data_device_manager = nullptr;
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

  for (output_t *output : d->outputs) {
    wl_output_destroy(output->output);
    delete output;
  }

  for (input_t *input : d->inputs) {
    if (input->data_source) {
      free(input->data_source->buffer_out);
      if (input->data_source->data_source) {
        wl_data_source_destroy(input->data_source->data_source);
      }
      delete input->data_source;
    }
    if (input->data_offer_copy_paste) {
      wl_data_offer_destroy(input->data_offer_copy_paste->id);
      delete input->data_offer_copy_paste;
    }
    if (input->data_device) {
      wl_data_device_release(input->data_device);
    }
    if (input->pointer) {
      if (input->cursor.file_buffer) {
        munmap(input->cursor.file_buffer->data, input->cursor.file_buffer->size);
        delete input->cursor.file_buffer;
      }
      if (input->cursor.surface) {
        wl_surface_destroy(input->cursor.surface);
      }
      if (input->cursor.theme) {
        wl_cursor_theme_destroy(input->cursor.theme);
      }
      if (input->pointer) {
        wl_pointer_destroy(input->pointer);
      }
    }
    if (input->keyboard) {
      if (input->key_repeat.timer) {
        delete static_cast<key_repeat_payload_t *>(input->key_repeat.timer->getUserData());
        input->system->removeTimer(input->key_repeat.timer);
        input->key_repeat.timer = nullptr;
      }
      wl_keyboard_destroy(input->keyboard);
    }
    if (input->xkb_state) {
      xkb_state_unref(input->xkb_state);
    }
    if (input->xkb_context) {
      xkb_context_unref(input->xkb_context);
    }
    wl_seat_destroy(input->seat);
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

static void relative_pointer_relative_motion(
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

  input->x += wl_fixed_to_int(dx);
  input->y += wl_fixed_to_int(dy);

  GHOST_IWindow *win = static_cast<GHOST_WindowWayland *>(
      wl_surface_get_user_data(input->focus_pointer));

  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 win,
                                                 input->x,
                                                 input->y,
                                                 GHOST_TABLET_DATA_NONE));
}

static const zwp_relative_pointer_v1_listener relative_pointer_listener = {
    relative_pointer_relative_motion,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Source), #wl_data_source_listener
 * \{ */

static void dnd_events(const input_t *const input, const GHOST_TEventType event)
{
  /* NOTE: `input->data_offer_dnd_mutex` must already be locked. */
  const uint64_t time = input->system->getMilliSeconds();
  GHOST_IWindow *const window = static_cast<GHOST_WindowWayland *>(
      wl_surface_get_user_data(input->focus_dnd));
  for (const std::string &type : mime_preference_order) {
    input->system->pushEvent(new GHOST_EventDragnDrop(time,
                                                      event,
                                                      mime_dnd.at(type),
                                                      window,
                                                      input->data_offer_dnd->dnd.x,
                                                      input->data_offer_dnd->dnd.y,
                                                      nullptr));
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
static void data_source_target(void * /*data*/,
                               struct wl_data_source * /*wl_data_source*/,
                               const char * /*mime_type*/)
{
  /* pass */
}

static void data_source_send(void *data,
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

static void data_source_cancelled(void * /*data*/, struct wl_data_source *wl_data_source)
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
static void data_source_dnd_drop_performed(void * /*data*/,
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
static void data_source_dnd_finished(void * /*data*/, struct wl_data_source * /*wl_data_source*/)
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
static void data_source_action(void * /*data*/,
                               struct wl_data_source * /*wl_data_source*/,
                               uint32_t /*dnd_action*/)
{
  /* pass */
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_target,
    data_source_send,
    data_source_cancelled,
    data_source_dnd_drop_performed,
    data_source_dnd_finished,
    data_source_action,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Offer), #wl_data_offer_listener
 * \{ */

static void data_offer_offer(void *data,
                             struct wl_data_offer * /*wl_data_offer*/,
                             const char *mime_type)
{
  static_cast<data_offer_t *>(data)->types.insert(mime_type);
}

static void data_offer_source_actions(void *data,
                                      struct wl_data_offer * /*wl_data_offer*/,
                                      uint32_t source_actions)
{
  static_cast<data_offer_t *>(data)->source_actions = source_actions;
}

static void data_offer_action(void *data,
                              struct wl_data_offer * /*wl_data_offer*/,
                              uint32_t dnd_action)
{
  static_cast<data_offer_t *>(data)->dnd_action = dnd_action;
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_offer,
    data_offer_source_actions,
    data_offer_action,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Data Device), #wl_data_device_listener
 * \{ */

static void data_device_data_offer(void * /*data*/,
                                   struct wl_data_device * /*wl_data_device*/,
                                   struct wl_data_offer *id)
{
  data_offer_t *data_offer = new data_offer_t;
  data_offer->id = id;
  wl_data_offer_add_listener(id, &data_offer_listener, data_offer);
}

static void data_device_enter(void *data,
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
  data_offer->dnd.x = wl_fixed_to_int(x);
  data_offer->dnd.y = wl_fixed_to_int(y);

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

static void data_device_leave(void *data, struct wl_data_device * /*wl_data_device*/)
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

static void data_device_motion(void *data,
                               struct wl_data_device * /*wl_data_device*/,
                               uint32_t /*time*/,
                               wl_fixed_t x,
                               wl_fixed_t y)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  input->data_offer_dnd->dnd.x = wl_fixed_to_int(x);
  input->data_offer_dnd->dnd.y = wl_fixed_to_int(y);
  dnd_events(input, GHOST_kEventDraggingUpdated);
}

static void data_device_drop(void *data, struct wl_data_device * /*wl_data_device*/)
{
  input_t *input = static_cast<input_t *>(data);
  std::lock_guard lock{input->data_offer_dnd_mutex};

  data_offer_t *data_offer = input->data_offer_dnd;

  const std::string mime_receive = *std::find_first_of(mime_preference_order.begin(),
                                                       mime_preference_order.end(),
                                                       data_offer->types.begin(),
                                                       data_offer->types.end());

  auto read_uris = [](input_t *const input,
                      data_offer_t *data_offer,
                      wl_surface *surface,
                      const std::string mime_receive) {
    const int x = data_offer->dnd.x;
    const int y = data_offer->dnd.y;

    const std::string data = read_pipe(data_offer, mime_receive, nullptr);

    wl_data_offer_finish(data_offer->id);
    wl_data_offer_destroy(data_offer->id);

    delete data_offer;
    data_offer = nullptr;

    GHOST_SystemWayland *const system = input->system;

    if (mime_receive == mime_text_uri) {
      static constexpr const char *file_proto = "file://";
      static constexpr const char *crlf = "\r\n";

      GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(get_window(surface));
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
      system->pushEvent(new GHOST_EventDragnDrop(system->getMilliSeconds(),
                                                 GHOST_kEventDraggingDropDone,
                                                 GHOST_kDragnDropTypeFilenames,
                                                 win,
                                                 x,
                                                 y,
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
  std::thread read_thread(read_uris, input, data_offer, input->focus_dnd, mime_receive);
  read_thread.detach();
}

static void data_device_selection(void *data,
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

  auto read_selection = [](input_t *input) {
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

  std::thread read_thread(read_selection, input);
  read_thread.detach();
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer,
    data_device_enter,
    data_device_leave,
    data_device_motion,
    data_device_drop,
    data_device_selection,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Surface), #wl_surface_listener
 * \{ */

static void cursor_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
  cursor_t *cursor = static_cast<cursor_t *>(data);

  wl_buffer_destroy(wl_buffer);

  if (wl_buffer == cursor->buffer) {
    /* the mapped buffer was from a custom cursor */
    cursor->buffer = nullptr;
  }
}

const struct wl_buffer_listener cursor_buffer_listener = {
    cursor_buffer_release,
};

static GHOST_IWindow *get_window(struct wl_surface *surface)
{
  if (!surface) {
    return nullptr;
  }

  for (GHOST_IWindow *win : window_manager->getWindows()) {
    if (surface == static_cast<const GHOST_WindowWayland *>(win)->surface()) {
      return win;
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
    wl_surface_set_buffer_scale(cursor.surface, scale);
    wl_cursor_theme_destroy(cursor.theme);
    cursor.theme = wl_cursor_theme_load(cursor.theme_name.c_str(), scale * cursor.size, shm);
    return true;
  }
  return false;
}

static void cursor_surface_enter(void *data,
                                 struct wl_surface * /*wl_surface*/,
                                 struct wl_output *output)
{
  input_t *input = static_cast<input_t *>(data);
  for (const output_t *reg_output : input->system->outputs()) {
    if (reg_output->output == output) {
      input->cursor.outputs.insert(reg_output);
    }
  }
  update_cursor_scale(input->cursor, input->system->shm());
}

static void cursor_surface_leave(void *data,
                                 struct wl_surface * /*wl_surface*/,
                                 struct wl_output *output)
{
  input_t *input = static_cast<input_t *>(data);
  for (const output_t *reg_output : input->system->outputs()) {
    if (reg_output->output == output) {
      input->cursor.outputs.erase(reg_output);
    }
  }
  update_cursor_scale(input->cursor, input->system->shm());
}

struct wl_surface_listener cursor_surface_listener = {
    cursor_surface_enter,
    cursor_surface_leave,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Pointer), #wl_pointer_listener
 * \{ */

static void pointer_enter(void *data,
                          struct wl_pointer * /*wl_pointer*/,
                          uint32_t serial,
                          struct wl_surface *surface,
                          wl_fixed_t surface_x,
                          wl_fixed_t surface_y)
{
  GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(get_window(surface));

  if (!win) {
    return;
  }

  win->activate();

  input_t *input = static_cast<input_t *>(data);
  input->pointer_serial = serial;
  input->x = win->scale() * wl_fixed_to_int(surface_x);
  input->y = win->scale() * wl_fixed_to_int(surface_y);
  input->focus_pointer = surface;

  win->setCursorShape(win->getCursorShape());

  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 static_cast<GHOST_WindowWayland *>(win),
                                                 input->x,
                                                 input->y,
                                                 GHOST_TABLET_DATA_NONE));
}

static void pointer_leave(void *data,
                          struct wl_pointer * /*wl_pointer*/,
                          uint32_t /*serial*/,
                          struct wl_surface *surface)
{
  GHOST_IWindow *win = get_window(surface);

  if (!win) {
    return;
  }

  static_cast<input_t *>(data)->focus_pointer = nullptr;
  static_cast<GHOST_WindowWayland *>(win)->deactivate();
}

static void pointer_motion(void *data,
                           struct wl_pointer * /*wl_pointer*/,
                           uint32_t /*time*/,
                           wl_fixed_t surface_x,
                           wl_fixed_t surface_y)
{
  input_t *input = static_cast<input_t *>(data);

  GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(get_window(input->focus_pointer));

  if (!win) {
    return;
  }

  input->x = win->scale() * wl_fixed_to_int(surface_x);
  input->y = win->scale() * wl_fixed_to_int(surface_y);

  input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                 GHOST_kEventCursorMove,
                                                 win,
                                                 input->x,
                                                 input->y,
                                                 GHOST_TABLET_DATA_NONE));
}

static void pointer_button(void *data,
                           struct wl_pointer * /*wl_pointer*/,
                           uint32_t serial,
                           uint32_t /*time*/,
                           uint32_t button,
                           uint32_t state)
{
  input_t *input = static_cast<input_t *>(data);

  GHOST_IWindow *win = get_window(input->focus_pointer);

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

static void pointer_axis(void *data,
                         struct wl_pointer * /*wl_pointer*/,
                         uint32_t /*time*/,
                         uint32_t axis,
                         wl_fixed_t value)
{
  input_t *input = static_cast<input_t *>(data);

  GHOST_IWindow *win = get_window(input->focus_pointer);

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
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Keyboard), #wl_keyboard_listener
 * \{ */

static void keyboard_keymap(
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

  input->xkb_state = xkb_state_new(keymap);

  xkb_keymap_unref(keymap);
}

/**
 * Enter event.
 *
 * Notification that this seat's keyboard focus is on a certain
 * surface.
 */
static void keyboard_enter(void *data,
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
static void keyboard_leave(void *data,
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

static void keyboard_key(void *data,
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
  const GHOST_TKey gkey = xkb_map_gkey(sym);

  /* Delete previous timer. */
  if (xkb_keymap_key_repeats(xkb_state_get_keymap(input->xkb_state), key + 8) &&
      input->key_repeat.timer) {
    delete static_cast<key_repeat_payload_t *>(input->key_repeat.timer->getUserData());
    input->system->removeTimer(input->key_repeat.timer);
    input->key_repeat.timer = nullptr;
  }

  GHOST_TEventKeyData key_data;

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
      input->system->getMilliSeconds(), etype, win, gkey, '\0', key_data.utf8_buf, false));

  /* Start timer for repeating key, if applicable. */
  if (input->key_repeat.rate > 0 &&
      xkb_keymap_key_repeats(xkb_state_get_keymap(input->xkb_state), key + 8) &&
      etype == GHOST_kEventKeyDown) {

    key_repeat_payload_t *payload = new key_repeat_payload_t({
        .system = input->system,
        .window = win,
        .key = gkey,
        .key_data = key_data,
    });

    auto cb = [](GHOST_ITimerTask *task, uint64_t /*time*/) {
      struct key_repeat_payload_t *payload = static_cast<key_repeat_payload_t *>(
          task->getUserData());
      payload->system->pushEvent(new GHOST_EventKey(payload->system->getMilliSeconds(),
                                                    GHOST_kEventKeyDown,
                                                    payload->window,
                                                    payload->key,
                                                    '\0',
                                                    payload->key_data.utf8_buf,
                                                    true));
    };
    input->key_repeat.timer = input->system->installTimer(
        input->key_repeat.delay, 1000 / input->key_repeat.rate, cb, payload);
  }
}

static void keyboard_modifiers(void *data,
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

static void keyboard_repeat_info(void *data,
                                 struct wl_keyboard * /*wl_keyboard*/,
                                 int32_t rate,
                                 int32_t delay)
{
  input_t *input = static_cast<input_t *>(data);

  input->key_repeat.rate = rate;
  input->key_repeat.delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Seat), #wl_seat_listener
 * \{ */

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
  input_t *input = static_cast<input_t *>(data);
  input->pointer = nullptr;
  input->keyboard = nullptr;

  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    input->pointer = wl_seat_get_pointer(wl_seat);
    input->cursor.surface = wl_compositor_create_surface(input->system->compositor());
    input->cursor.visible = true;
    input->cursor.buffer = nullptr;
    input->cursor.file_buffer = new buffer_t;
    if (!get_cursor_settings(input->cursor.theme_name, input->cursor.size)) {
      input->cursor.theme_name = std::string();
      input->cursor.size = default_cursor_size;
    }
    wl_pointer_add_listener(input->pointer, &pointer_listener, data);
    wl_surface_add_listener(input->cursor.surface, &cursor_surface_listener, data);
  }

  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    input->keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(input->keyboard, &keyboard_listener, data);
  }
}

static void seat_name(void *data, struct wl_seat * /*wl_seat*/, const char *name)
{
  static_cast<input_t *>(data)->name = std::string(name);
}

static const struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Output), #wl_output_listener
 * \{ */

static void output_geometry(void *data,
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
  output->width_mm = physical_width;
  output->height_mm = physical_height;
}

static void output_mode(void *data,
                        struct wl_output * /*wl_output*/,
                        uint32_t /*flags*/,
                        int32_t width,
                        int32_t height,
                        int32_t /*refresh*/)
{
  output_t *output = static_cast<output_t *>(data);
  output->width_pxl = width;
  output->height_pxl = height;
}

/**
 * Sent all information about output.
 *
 * This event is sent after all other properties have been sent
 * after binding to the output object and after any other property
 * changes done after that. This allows changes to the output
 * properties to be seen as atomic, even if they happen via multiple events.
 */
static void output_done(void * /*data*/, struct wl_output * /*wl_output*/)
{
}

static void output_scale(void *data, struct wl_output * /*wl_output*/, int32_t factor)
{
  static_cast<output_t *>(data)->scale = factor;
}

static const struct wl_output_listener output_listener = {
    output_geometry,
    output_mode,
    output_done,
    output_scale,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (XDG WM Base), #xdg_wm_base_listener
 * \{ */

static void shell_ping(void * /*data*/, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener shell_listener = {
    shell_ping,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Listener (Registry), #wl_registry_listener
 * \{ */

static void global_add(void *data,
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
  else if (!strcmp(interface, wl_output_interface.name)) {
    output_t *output = new output_t;
    output->scale = 1;
    output->output = static_cast<wl_output *>(
        wl_registry_bind(wl_registry, name, &wl_output_interface, 2));
    display->outputs.push_back(output);
    wl_output_add_listener(output->output, &output_listener, output);
  }
  else if (!strcmp(interface, wl_seat_interface.name)) {
    input_t *input = new input_t;
    input->system = display->system;
    input->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    input->xkb_state = nullptr;
    input->data_offer_dnd = nullptr;
    input->data_offer_copy_paste = nullptr;
    input->data_source = new data_source_t;
    input->data_source->data_source = nullptr;
    input->data_source->buffer_out = nullptr;
    input->relative_pointer = nullptr;
    input->locked_pointer = nullptr;
    input->confined_pointer = nullptr;
    input->seat = static_cast<wl_seat *>(
        wl_registry_bind(wl_registry, name, &wl_seat_interface, 4));
    display->inputs.push_back(input);
    wl_seat_add_listener(input->seat, &seat_listener, input);
  }
  else if (!strcmp(interface, wl_shm_interface.name)) {
    display->shm = static_cast<wl_shm *>(
        wl_registry_bind(wl_registry, name, &wl_shm_interface, 1));
  }
  else if (!strcmp(interface, wl_data_device_manager_interface.name)) {
    display->data_device_manager = static_cast<wl_data_device_manager *>(
        wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 3));
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
static void global_remove(void * /*data*/, struct wl_registry * /*wl_registry*/, uint32_t /*name*/)
{
}

static const struct wl_registry_listener registry_listener = {
    global_add,
    global_remove,
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
                                                                  input->seat);
      wl_data_device_add_listener(input->data_device, &data_device_listener, input);
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
  if (d->inputs.empty() || (d->inputs[0]->focus_pointer == nullptr)) {
    return GHOST_kFailure;
  }

  x = d->inputs[0]->x;
  y = d->inputs[0]->y;
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
    width = uint32_t(d->outputs[0]->width_pxl) / d->outputs[0]->scale;
    height = uint32_t(d->outputs[0]->height_pxl) / d->outputs[0]->scale;
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

xdg_wm_base *GHOST_SystemWayland::shell()
{
  return d->xdg_shell;
}

zxdg_decoration_manager_v1 *GHOST_SystemWayland::decoration_manager()
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

  const int32_t image_size_x = int32_t(c->image.width);
  const int32_t image_size_y = int32_t(c->image.height);

  const int32_t hotspot_x = int32_t(c->image.hotspot_x) / c->scale;
  const int32_t hotspot_y = int32_t(c->image.hotspot_y) / c->scale;

  wl_surface_attach(c->surface, buffer, 0, 0);
  wl_surface_damage(c->surface, 0, 0, image_size_x, image_size_y);

  wl_pointer_set_cursor(input->pointer,
                        input->pointer_serial,
                        c->visible ? c->surface : nullptr,
                        hotspot_x,
                        hotspot_y);

  wl_surface_commit(c->surface);
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

  if (!c->theme) {
    /* The cursor surface hasn't entered an output yet. Initialize theme with scale 1. */
    c->theme = wl_cursor_theme_load(c->theme_name.c_str(), c->size, d->inputs[0]->system->shm());
  }

  wl_cursor *cursor = wl_cursor_theme_get_cursor(c->theme, cursor_name.c_str());

  if (!cursor) {
    GHOST_PRINT("cursor '" << cursor_name << "' does not exist" << std::endl);
    return GHOST_kFailure;
  }

  struct wl_cursor_image *image = cursor->images[0];
  struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
  if (!buffer) {
    return GHOST_kFailure;
  }

  c->buffer = buffer;
  c->image = *image;

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

  cursor->buffer = buffer;
  cursor->image.width = uint32_t(sizex);
  cursor->image.height = uint32_t(sizey);
  cursor->image.hotspot_x = uint32_t(hotX);
  cursor->image.hotspot_y = uint32_t(hotY);

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
      set_cursor_buffer(input, cursor->buffer);
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
        GHOST_WindowWayland *win = static_cast<GHOST_WindowWayland *>(get_window(surface));
        GHOST_Rect bounds;
        int x_new = input->x, y_new = input->y;

        /* Fallback to window bounds. */
        if (win->getCursorGrabBounds(bounds) == GHOST_kFailure) {
          ((GHOST_Window *)win)->getClientBounds(bounds);
        }
        bounds.wrapPoint(x_new, y_new, 0, win->getCursorGrabAxis());

        /* Push an event so the new location is registered. */
        if ((x_new != input->x) || (y_new != input->y)) {
          input->system->pushEvent(new GHOST_EventCursor(input->system->getMilliSeconds(),
                                                         GHOST_kEventCursorMove,
                                                         win,
                                                         x_new,
                                                         y_new,
                                                         GHOST_TABLET_DATA_NONE));
        }
        input->x = x_new;
        input->y = y_new;

        const int scale = win->scale();
        zwp_locked_pointer_v1_set_cursor_position_hint(input->locked_pointer,
                                                       wl_fixed_from_int(x_new) / scale,
                                                       wl_fixed_from_int(y_new) / scale);
        wl_surface_commit(surface);
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
            d->relative_pointer_manager, input->pointer);
        zwp_relative_pointer_v1_add_listener(
            input->relative_pointer, &relative_pointer_listener, input);
        input->locked_pointer = zwp_pointer_constraints_v1_lock_pointer(
            d->pointer_constraints,
            surface,
            input->pointer,
            nullptr,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
      }
    }
    else if (use_confine) {
      if (!was_confine) {
        input->confined_pointer = zwp_pointer_constraints_v1_confine_pointer(
            d->pointer_constraints,
            surface,
            input->pointer,
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
