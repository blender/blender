/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"
#include "DNA_screen_types.h" /* for #ScrAreaMap */
#include "DNA_xr_types.h"     /* for #XrSessionSettings */

#include "DNA_ID.h"

#ifdef __cplusplus
#  include <mutex>
using std_mutex_type = std::mutex;
#else
#  define std_mutex_type void
#endif

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus
namespace blender::bke {
class WindowManagerRuntime;
}
using WindowManagerRuntimeHandle = blender::bke::WindowManagerRuntime;
#else   // __cplusplus
typedef struct WindowManagerRuntimeHandle WindowManagerRuntimeHandle;
#endif  // __cplusplus

/* Defined here: */

struct wmWindow;
struct wmWindowManager;

struct wmEvent_ConsecutiveData;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmMsgBus;
struct wmOperator;
struct wmOperatorType;

/* Forward declarations: */

struct PointerRNA;
struct Report;
struct ReportList;
struct Stereo3dFormat;
struct bContext;
struct bScreen;
struct uiLayout;
struct wmTimer;

#define OP_MAX_TYPENAME 64
#define KMAP_MAX_NAME 64

/** Keep in sync with 'rna_enum_wm_report_items' in `wm_rna.c`. */
typedef enum eReportType {
  RPT_DEBUG = (1 << 0),
  RPT_INFO = (1 << 1),
  RPT_OPERATOR = (1 << 2),
  RPT_PROPERTY = (1 << 3),
  RPT_WARNING = (1 << 4),
  RPT_ERROR = (1 << 5),
  RPT_ERROR_INVALID_INPUT = (1 << 6),
  RPT_ERROR_INVALID_CONTEXT = (1 << 7),
  RPT_ERROR_OUT_OF_MEMORY = (1 << 8),
} eReportType;
ENUM_OPERATORS(eReportType, RPT_ERROR_OUT_OF_MEMORY)

#define RPT_DEBUG_ALL (RPT_DEBUG)
#define RPT_INFO_ALL (RPT_INFO)
#define RPT_OPERATOR_ALL (RPT_OPERATOR)
#define RPT_PROPERTY_ALL (RPT_PROPERTY)
#define RPT_WARNING_ALL (RPT_WARNING)
#define RPT_ERROR_ALL \
  (RPT_ERROR | RPT_ERROR_INVALID_INPUT | RPT_ERROR_INVALID_CONTEXT | RPT_ERROR_OUT_OF_MEMORY)

enum ReportListFlags {
  RPT_PRINT = (1 << 0),
  RPT_STORE = (1 << 1),
  RPT_FREE = (1 << 2),
  RPT_OP_HOLD = (1 << 3), /* don't move them into the operator global list (caller will use) */
  /** Don't print (the owner of the #ReportList will handle printing to the `stdout`). */
  RPT_PRINT_HANDLED_BY_OWNER = (1 << 4),
};

/* These two lines with # tell `makesdna` this struct can be excluded. */
#
#
typedef struct Report {
  struct Report *next, *prev;
  /** eReportType. */
  short type;
  short flag;
  /** `strlen(message)`, saves some time calculating the word wrap. */
  int len;
  const char *typestr;
  const char *message;
} Report;

/**
 * \note Saved in the #wmWindowManager, don't remove.
 */
typedef struct ReportList {
  ListBase list;
  /** #eReportType. */
  int printlevel;
  /** #eReportType. */
  int storelevel;
  int flag;
  char _pad[4];
  struct wmTimer *reporttimer;

  /** Mutex for thread-safety, runtime only. */
  std_mutex_type *lock;
} ReportList;

/* Timer custom-data to control reports display. */
/* These two lines with # tell `makesdna` this struct can be excluded. */
#
#
typedef struct ReportTimerInfo {
  float widthfac;
  float flash_progress;
} ReportTimerInfo;

// #ifdef WITH_XR_OPENXR
typedef struct wmXrData {
  /** Runtime information for managing Blender specific behaviors. */
  struct wmXrRuntimeData *runtime;
  /** Permanent session settings (draw mode, feature toggles, etc). Stored in files and accessible
   * even before the session runs. */
  XrSessionSettings session_settings;
} wmXrData;
// #endif

/* reports need to be before wmWindowManager */

/** Window-manager is saved, tag WMAN. */
typedef struct wmWindowManager {
  ID id;

  /** Separate active from drawable. */
  struct wmWindow *windrawable;
  /**
   * \note `CTX_wm_window(C)` is usually preferred.
   * Avoid relying on this where possible as this may become NULL during when handling
   * events that close or replace windows (opening a file for e.g.).
   * While this happens rarely in practice, it can cause difficult to reproduce bugs.
   */
  struct wmWindow *winactive;
  ListBase windows;

  /** Set on file read. */
  uint8_t init_flag;
  char _pad0[1];
  /** Indicator whether data was saved. */
  short file_saved;
  /** Operator stack depth to avoid nested undo pushes. */
  short op_undo_depth;

  /** Set after selection to notify outliner to sync. Stores type of selection */
  short outliner_sync_select_dirty;

  /** Operator registry. */
  ListBase operators;

  /**
   * Refresh/redraw #wmNotifier structs.
   * \note Once in the queue, notifiers should be considered read-only.
   * With the exception of clearing notifiers for data which has been removed,
   * see: #NOTE_CATEGORY_TAG_CLEARED.
   */
  ListBase notifier_queue;
  /**
   * For duplicate detection.
   * \note keep in sync with `notifier_queue` adding/removing elements must also update this set.
   */
  struct GSet *notifier_queue_set;
  void *_pad1;

  /** Information and error reports. */
  struct ReportList reports;

  /** Threaded jobs manager. */
  ListBase jobs;

  /** Extra overlay cursors to draw, like circles. */
  ListBase paintcursors;

  /** Active dragged items. */
  ListBase drags;

  /**
   * Known key configurations.
   * This includes all the #wmKeyConfig members (`defaultconf`, `addonconf`, etc).
   */
  ListBase keyconfigs;

  /** Default configuration. */
  struct wmKeyConfig *defaultconf;
  /** Addon configuration. */
  struct wmKeyConfig *addonconf;
  /** User configuration. */
  struct wmKeyConfig *userconf;

  /** Active timers. */
  ListBase timers;
  /** Timer for auto save. */
  struct wmTimer *autosavetimer;

  /** All undo history (runtime only). */
  struct UndoStack *undo_stack;

  struct wmMsgBus *message_bus;

  // #ifdef WITH_XR_OPENXR
  wmXrData xr;
  // #endif

  WindowManagerRuntimeHandle *runtime;
} wmWindowManager;

#define WM_KEYCONFIG_ARRAY_P(wm) &(wm)->defaultconf, &(wm)->addonconf, &(wm)->userconf

/** #wmWindowManager.init_flag */
enum {
  WM_INIT_FLAG_WINDOW = (1 << 0),
  WM_INIT_FLAG_KEYCONFIG = (1 << 1),
};

/** #wmWindowManager.outliner_sync_select_dirty */
enum {
  WM_OUTLINER_SYNC_SELECT_FROM_OBJECT = (1 << 0),
  WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE = (1 << 1),
  WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE = (1 << 2),
  WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE = (1 << 3),
};

#define WM_OUTLINER_SYNC_SELECT_FROM_ALL \
  (WM_OUTLINER_SYNC_SELECT_FROM_OBJECT | WM_OUTLINER_SYNC_SELECT_FROM_EDIT_BONE | \
   WM_OUTLINER_SYNC_SELECT_FROM_POSE_BONE | WM_OUTLINER_SYNC_SELECT_FROM_SEQUENCE)

#define WM_KEYCONFIG_STR_DEFAULT "Blender"

/* IME is win32 and apple only! */
#if !(defined(WIN32) || defined(__APPLE__)) && !defined(DNA_DEPRECATED)
#  ifdef __GNUC__
#    define ime_data ime_data __attribute__((deprecated))
#  endif
#endif

/**
 * The saveable part, the rest of the data is local in GHOST.
 */
typedef struct wmWindow {
  struct wmWindow *next, *prev;

  /** Don't want to include ghost.h stuff. */
  void *ghostwin;
  /** Don't want to include gpu stuff. */
  void *gpuctx;

  /** Parent window. */
  struct wmWindow *parent;

  /** Active scene displayed in this window. */
  struct Scene *scene;
  /** Temporary when switching. */
  struct Scene *new_scene;
  /** Active view layer displayed in this window. */
  char view_layer_name[64];
  /** The workspace may temporarily override the window's scene with scene pinning. This is the
   * "overridden" or "default" scene to restore when entering a workspace with no scene pinned. */
  struct Scene *unpinned_scene;

  struct WorkSpaceInstanceHook *workspace_hook;

  /** Global areas aren't part of the screen, but part of the window directly.
   * \note Code assumes global areas with fixed height, fixed width not supported yet */
  ScrAreaMap global_areas;

  struct bScreen *screen DNA_DEPRECATED;

  /** Window-ID also in screens, is for retrieving this window after read. */
  int winid;
  /** Window coords (in pixels). */
  short posx, posy;
  /**
   * Window size (in pixels).
   *
   * \note Loading a window typically uses the size & position saved in the blend-file,
   * there is an exception for startup files which works as follows:
   * Setting the window size to zero before `ghostwin` has been set has a special meaning,
   * it causes the window size to be initialized to `wm_init_state.size_x` (& `size_y`).
   * These default to the main screen size but can be overridden by the `--window-geometry`
   * command line argument.
   */
  short sizex, sizey;
  /** Normal, maximized, full-screen, #GHOST_TWindowState. */
  char windowstate;
  /** Set to 1 if an active window, for quick rejects. */
  char active;
  /** Current mouse cursor type. */
  short cursor;
  /** Previous cursor when setting modal one. */
  short lastcursor;
  /** The current modal cursor. */
  short modalcursor;
  /** Cursor grab mode #GHOST_TGrabCursorMode (run-time only) */
  short grabcursor;
  /** Internal: tag this for extra mouse-move event,
   * makes cursors/buttons active on UI switching. */

  /** Internal, lock pie creation from this event until released. */
  short pie_event_type_lock;
  /**
   * Exception to the above rule for nested pies, store last pie event for operators
   * that spawn a new pie right after destruction of last pie.
   */
  short pie_event_type_last;

  char addmousemove;
  char tag_cursor_refresh;

  /* Track the state of the event queue,
   * these store the state that needs to be kept between handling events in the queue. */
  /** Enable when #KM_PRESS events are not handled (keyboard/mouse-buttons only). */
  char event_queue_check_click;
  /** Enable when #KM_PRESS events are not handled (keyboard/mouse-buttons only). */
  char event_queue_check_drag;
  /**
   * Enable when the drag was handled,
   * to avoid mouse-motion continually triggering drag events which are not handled
   * but add overhead to gizmo handling (for example), see #87511.
   */
  char event_queue_check_drag_handled;

  /** The last event type (that passed #WM_event_consecutive_gesture_test check). */
  char event_queue_consecutive_gesture_type;
  /** The cursor location when `event_queue_consecutive_gesture_type` was set. */
  int event_queue_consecutive_gesture_xy[2];
  /** See #WM_event_consecutive_data_get and related API. Freed when consecutive events end. */
  struct wmEvent_ConsecutiveData *event_queue_consecutive_gesture_data;

  /**
   * Storage for event system.
   *
   * For the most part this is storage for `wmEvent.xy` & `wmEvent.modifiers`.
   * newly added key/button events copy the cursor location and modifier state stored here.
   *
   * It's also convenient at times to be able to pass this as if it's a regular event.
   *
   * - This is not simply the current event being handled.
   *   The type and value is always set to the last press/release events
   *   otherwise cursor motion would always clear these values.
   *
   * - The value of `eventstate->modifiers` is set from the last pressed/released modifier key.
   *   This has the down side that the modifier value will be incorrect if users hold both
   *   left/right modifiers then release one. See note in #wm_event_add_ghostevent for details.
   */
  struct wmEvent *eventstate;
  /**
   * Keep the last handled event in `event_queue` here (owned and must be freed).
   *
   * \warning This must only to be used for event queue logic.
   * User interactions should use `eventstate` instead (if the event isn't passed to the function).
   */
  struct wmEvent *event_last_handled;

  /**
   * Input Method Editor data - complex character input (especially for Asian character input)
   * Currently WIN32 and APPLE, runtime-only data.
   */
  const struct wmIMEData *ime_data;
  char ime_data_is_composing;
  char _pad1[7];

  /** All events #wmEvent (ghost level events were handled). */
  ListBase event_queue;
  /** Window+screen handlers, handled last. */
  ListBase handlers;
  /** Priority handlers, handled first. */
  ListBase modalhandlers;

  /** Gesture stuff. */
  ListBase gesture;

  /** Properties for stereoscopic displays. */
  struct Stereo3dFormat *stereo3d_format;

  /** Custom drawing callbacks. */
  ListBase drawcalls;

  /** Private runtime info to show text in the status bar. */
  void *cursor_keymap_status;

  /**
   * The time when the key is pressed in milliseconds (see #GHOST_GetEventTime).
   * Used to detect double-click events.
   */
  uint64_t eventstate_prev_press_time_ms;

} wmWindow;

#ifdef ime_data
#  undef ime_data
#endif

/* These two lines with # tell `makesdna` this struct can be excluded. */
/* should be something like DNA_EXCLUDE
 * but the preprocessor first removes all comments, spaces etc */
#
#
typedef struct wmOperatorTypeMacro {
  struct wmOperatorTypeMacro *next, *prev;

  /* operator id */
  char idname[64]; /* OP_MAX_TYPENAME */
  /* rna pointer to access properties, like keymap */
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  struct IDProperty *properties;
  struct PointerRNA *ptr;
} wmOperatorTypeMacro;

/**
 * Partial copy of the event, for matching by event handler.
 */
typedef struct wmKeyMapItem {
  struct wmKeyMapItem *next, *prev;

  /* operator */
  /** Used to retrieve operator type pointer. */
  char idname[64];
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  IDProperty *properties;

  /* modal */
  /** Runtime temporary storage for loading. */
  char propvalue_str[64];
  /** If used, the item is from modal map. */
  short propvalue;

  /* event */
  /** Event code itself (#EVT_LEFTCTRLKEY, #LEFTMOUSE etc). */
  short type;
  /** Button state (#KM_ANY, #KM_PRESS, #KM_DBL_CLICK, #KM_CLICK_DRAG, #KM_NOTHING etc). */
  int8_t val;
  /**
   * The 2D direction of the event to use when `val == KM_CLICK_DRAG`.
   * Set to #KM_DIRECTION_N, #KM_DIRECTION_S & related values, #KM_NOTHING for any direction.
   */
  int8_t direction;
  /** `oskey` also known as apple, windows-key or super. */
  short shift, ctrl, alt, oskey;
  /** Raw-key modifier. */
  short keymodifier;

  /* flag: inactive, expanded */
  short flag;

  /* runtime */
  /** Keymap editor. */
  short maptype;
  /** Unique identifier. Positive for kmi that override builtins, negative otherwise. */
  short id;
  char _pad[2];
  /**
   * RNA pointer to access properties.
   *
   * \note The `ptr.owner_id` value must be NULL, as a signal not to use the context
   * when running property callbacks such as ENUM item functions.
   */
  struct PointerRNA *ptr;
} wmKeyMapItem;

/** Used instead of wmKeyMapItem for diff keymaps. */
typedef struct wmKeyMapDiffItem {
  struct wmKeyMapDiffItem *next, *prev;

  wmKeyMapItem *remove_item;
  wmKeyMapItem *add_item;
} wmKeyMapDiffItem;

/** #wmKeyMapItem.flag */
enum {
  KMI_INACTIVE = (1 << 0),
  KMI_EXPANDED = (1 << 1),
  KMI_USER_MODIFIED = (1 << 2),
  KMI_UPDATE = (1 << 3),
  /**
   * When set, ignore events with `wmEvent.flag & WM_EVENT_IS_REPEAT` enabled.
   *
   * \note this flag isn't cleared when editing/loading the key-map items,
   * so it may be set in cases which don't make sense (modifier-keys or mouse-motion for example).
   *
   * Knowing if an event may repeat is something set at the operating-systems event handling level
   * so rely on #WM_EVENT_IS_REPEAT being false non keyboard events instead of checking if this
   * flag makes sense.
   *
   * Only used when: `ISKEYBOARD(kmi->type) || (kmi->type == KM_TEXTINPUT)`
   * as mouse, 3d-mouse, timer... etc never repeat.
   */
  KMI_REPEAT_IGNORE = (1 << 4),
};

/** #wmKeyMapItem.maptype */
enum {
  KMI_TYPE_KEYBOARD = 0,
  KMI_TYPE_MOUSE = 1,
  /* 2 is deprecated, was tweak. */
  KMI_TYPE_TEXTINPUT = 3,
  KMI_TYPE_TIMER = 4,
  KMI_TYPE_NDOF = 5,
};

/**
 * Stored in WM, the actively used key-maps.
 */
typedef struct wmKeyMap {
  struct wmKeyMap *next, *prev;

  ListBase items;
  ListBase diff_items;

  /** Global editor keymaps, or for more per space/region. */
  char idname[64];
  /** Same IDs as in DNA_space_types.h. */
  short spaceid;
  /** See above. */
  short regionid;
  /** Optional, see: #wmOwnerID. */
  char owner_id[64];

  /** General flags. */
  short flag;
  /** Last kmi id. */
  short kmi_id;

  /* runtime */
  /** Verify if enabled in the current context, use #WM_keymap_poll instead of direct calls. */
  bool (*poll)(struct bContext *);
  bool (*poll_modal_item)(const struct wmOperator *op, int value);

  /** For modal, #EnumPropertyItem for now. */
  const void *modal_items;
} wmKeyMap;

/** #wmKeyMap.flag */
enum {
  /** Modal map, not using operator-names. */
  KEYMAP_MODAL = (1 << 0),
  /** User key-map. */
  KEYMAP_USER = (1 << 1),
  KEYMAP_EXPANDED = (1 << 2),
  KEYMAP_CHILDREN_EXPANDED = (1 << 3),
  /** Diff key-map for user preferences. */
  KEYMAP_DIFF = (1 << 4),
  /** Key-map has user modifications. */
  KEYMAP_USER_MODIFIED = (1 << 5),
  KEYMAP_UPDATE = (1 << 6),
  /** key-map for active tool system. */
  KEYMAP_TOOL = (1 << 7),
};

/**
 * This is similar to addon-preferences,
 * however unlike add-ons key-configurations aren't saved to disk.
 *
 * #wmKeyConfigPref is written to DNA,
 * #wmKeyConfigPrefType_Runtime has the RNA type.
 */
typedef struct wmKeyConfigPref {
  struct wmKeyConfigPref *next, *prev;
  /** Unique name. */
  char idname[64];
  IDProperty *prop;
} wmKeyConfigPref;

typedef struct wmKeyConfig {
  struct wmKeyConfig *next, *prev;

  /** Unique name. */
  char idname[64];
  /** ID-name of configuration this is derives from, "" if none. */
  char basename[64];

  ListBase keymaps;
  int actkeymap;
  short flag;
  char _pad0[2];
} wmKeyConfig;

/** #wmKeyConfig.flag */
enum {
  KEYCONF_USER = (1 << 1),         /* And what about (1 << 0)? */
  KEYCONF_INIT_DEFAULT = (1 << 2), /* Has default keymap been initialized? */
};

/**
 * This one is the operator itself, stored in files for macros etc.
 * operator + operator-type should be able to redo entirely, but for different context's.
 */
typedef struct wmOperator {
  struct wmOperator *next, *prev;

  /* saved */
  /** Used to retrieve type pointer. */
  char idname[64]; /* OP_MAX_TYPENAME */
  /** Saved, user-settable properties. */
  IDProperty *properties;

  /* runtime */
  /** Operator type definition from idname. */
  struct wmOperatorType *type;
  /** Custom storage, only while operator runs. */
  void *customdata;
  /** Python stores the class instance here. */
  void *py_instance;

  /** Rna pointer to access properties. */
  struct PointerRNA *ptr;
  /** Errors and warnings storage. */
  struct ReportList *reports;

  /** List of operators, can be a tree. */
  ListBase macro;
  /** Current running macro, not saved. */
  struct wmOperator *opm;
  /** Runtime for drawing. */
  struct uiLayout *layout;
  short flag;
  char _pad[6];
} wmOperator;

/**
 * Operator type return flags: exec(), invoke() modal(), return values.
 */
enum {
  OPERATOR_RUNNING_MODAL = (1 << 0),
  OPERATOR_CANCELLED = (1 << 1),
  OPERATOR_FINISHED = (1 << 2),
  /** Add this flag if the event should pass through. */
  OPERATOR_PASS_THROUGH = (1 << 3),
  /** In case operator got executed outside WM code (like via file-select). */
  OPERATOR_HANDLED = (1 << 4),
  /**
   * Used for operators that act indirectly (eg. popup menu).
   * \note this isn't great design (using operators to trigger UI) avoid where possible.
   */
  OPERATOR_INTERFACE = (1 << 5),
};
#define OPERATOR_FLAGS_ALL \
  (OPERATOR_RUNNING_MODAL | OPERATOR_CANCELLED | OPERATOR_FINISHED | OPERATOR_PASS_THROUGH | \
   OPERATOR_HANDLED | OPERATOR_INTERFACE | 0)

/* sanity checks for debug mode only */
#define OPERATOR_RETVAL_CHECK(ret) \
  (void)ret, BLI_assert(ret != 0 && (ret & OPERATOR_FLAGS_ALL) == ret)

/** #wmOperator.flag */
enum {
  /**
   * Low level flag so exec() operators can tell if they were invoked, use with care.
   * Typically this shouldn't make any difference, but it rare cases its needed (see smooth-view).
   */
  OP_IS_INVOKE = (1 << 0),
  /** So we can detect if an operators exec() call is activated by adjusting the last action. */
  OP_IS_REPEAT = (1 << 1),
  /**
   * So we can detect if an operators exec() call is activated from #SCREEN_OT_repeat_last.
   *
   * This difference can be important because previous settings may be used,
   * even with #PROP_SKIP_SAVE the repeat last operator will use the previous settings.
   * Unlike #OP_IS_REPEAT the selection (and context generally) may be different each time.
   * See #60777 for an example of when this is needed.
   */
  OP_IS_REPEAT_LAST = (1 << 1),

  /** When the cursor is grabbed */
  OP_IS_MODAL_GRAB_CURSOR = (1 << 2),

  /**
   * Allow modal operators to have the region under the cursor for their context
   * (the region-type is maintained to prevent errors).
   */
  OP_IS_MODAL_CURSOR_REGION = (1 << 3),
};
