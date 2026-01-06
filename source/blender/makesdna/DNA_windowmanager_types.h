/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_windowmanager_enums.h" /* Own enums. */

#include "DNA_listBase.h"
#include "DNA_screen_types.h" /* for #ScrAreaMap */
#include "DNA_xr_types.h"     /* for #XrSessionSettings */

#include "DNA_ID.h"

namespace blender {

/** Workaround to forward-declare C++ type in C header. */
namespace bke {
struct WindowManagerRuntime;
struct WindowRuntime;
}  // namespace bke

namespace ui {
struct Layout;
}  // namespace ui

#ifdef hyper /* MSVC defines. */
#  undef hyper
#endif

/* Defined here: */

struct wmNotifier;
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
struct wmTimer;

#define OP_MAX_TYPENAME 64
#define KMAP_MAX_NAME 64

/* Timer custom-data to control reports display. */
/* These two lines with # tell `makesdna` this struct can be excluded. */
#
#
struct ReportTimerInfo {
  float widthfac = 0;
  float flash_progress = 0;
};

/* reports need to be before wmWindowManager */

// #ifdef WITH_XR_OPENXR
struct wmXrData {
  /** Runtime information for managing Blender specific behaviors. */
  struct wmXrRuntimeData *runtime = nullptr;
  /** Permanent session settings (draw mode, feature toggles, etc). Stored in files and accessible
   * even before the session runs. */
  XrSessionSettings session_settings;
};
// #endif

/** #wmWindowManager.extensions_updates */
enum {
  WM_EXTENSIONS_UPDATE_UNSET = -2,
  WM_EXTENSIONS_UPDATE_CHECKING = -1,
};

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

/** Window-manager is saved, tag WMAN. */
struct wmWindowManager {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_WM;
#endif

  ID id;

  ListBaseT<wmWindow> windows = {nullptr, nullptr};

  /** Set on file read. */
  uint8_t init_flag = 0;
  char _pad0[1] = {};
  /** Indicator whether data was saved. */
  short file_saved = 0;
  /** Operator stack depth to avoid nested undo pushes. */
  short op_undo_depth = 0;

  /** Set after selection to notify outliner to sync. Stores type of selection */
  short outliner_sync_select_dirty = 0;

  /** Available/pending extensions updates. */
  int extensions_updates = 0;
  /** Number of blocked & installed extensions. */
  int extensions_blocked = 0;

  /** Timer for auto save. */
  struct wmTimer *autosavetimer = nullptr;
  /** Auto-save timer was up, but it wasn't possible to auto-save in the current mode. */
  char autosave_scheduled = 0;
  char _pad2[7] = {};

  // #ifdef WITH_XR_OPENXR
  wmXrData xr;
  // #endif

  bke::WindowManagerRuntime *runtime = nullptr;
};

#define WM_KEYCONFIG_ARRAY_P(wm) \
  &(wm)->runtime->defaultconf, &(wm)->runtime->addonconf, &(wm)->runtime->userconf

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
struct wmWindow {
  DNA_DEFINE_CXX_METHODS(wmWindow)

  struct wmWindow *next = nullptr, *prev = nullptr;

  /** Parent window. */
  struct wmWindow *parent = nullptr;

  /** Active scene displayed in this window. */
  struct Scene *scene = nullptr;
  /** Temporary when switching. */
  struct Scene *new_scene = nullptr;
  /** Active view layer displayed in this window. */
  char view_layer_name[/*MAX_NAME*/ 64] = "";
  /** The workspace may temporarily override the window's scene with scene pinning. This is the
   * "overridden" or "default" scene to restore when entering a workspace with no scene pinned. */
  struct Scene *unpinned_scene = nullptr;

  struct WorkSpaceInstanceHook *workspace_hook = nullptr;

  /** Global areas aren't part of the screen, but part of the window directly.
   * \note Code assumes global areas with fixed height, fixed width not supported yet */
  ScrAreaMap global_areas;

  DNA_DEPRECATED struct bScreen *screen = nullptr;

  /** Window-ID also in screens, is for retrieving this window after read. */
  int winid = 0;
  /** Window coords (in pixels). */
  short posx = 0, posy = 0;
  /**
   * Window size (in pixels).
   *
   * \note Loading a window typically uses the size & position saved in the blend-file,
   * there is an exception for startup files which works as follows:
   * Setting the window size to zero before `ghostwin` has been set has a special meaning,
   * it causes the window size to be initialized to `wm_init_state.size`.
   * These default to the main screen size but can be overridden by the `--window-geometry`
   * command line argument.
   *
   * \warning Using these values directly can result in errors on macOS due to HiDPI displays
   * influencing the window native pixel size. See #WM_window_native_pixel_size for a general use
   * alternative.
   */
  short sizex = 0, sizey = 0;
  /** Normal, maximized, full-screen, #GHOST_TWindowState. */
  char windowstate = 0;
  /** Set to 1 if an active window, for quick rejects. */
  char active = 0;
  /** Current mouse cursor type. */
  short cursor = 0;
  /** Previous cursor when setting modal one. */
  short lastcursor = 0;
  /** The current modal cursor. */
  short modalcursor = 0;
  /** Cursor grab mode #GHOST_TGrabCursorMode (run-time only) */
  short grabcursor = 0;

  /** Internal, lock pie creation from this event until released. */
  short pie_event_type_lock = 0;
  /**
   * Exception to the above rule for nested pies, store last pie event for operators
   * that spawn a new pie right after destruction of last pie.
   */
  short pie_event_type_last = 0;

  char tag_cursor_refresh = 0;

  /* Track the state of the event queue,
   * these store the state that needs to be kept between handling events in the queue. */
  /** Enable when #KM_PRESS events are not handled (keyboard/mouse-buttons only). */
  char event_queue_check_click = 0;
  /** Enable when #KM_PRESS events are not handled (keyboard/mouse-buttons only). */
  char event_queue_check_drag = 0;
  /**
   * Enable when the drag was handled,
   * to avoid mouse-motion continually triggering drag events which are not handled
   * but add overhead to gizmo handling (for example), see #87511.
   */
  char event_queue_check_drag_handled = 0;

  /**
   * The last event type (that passed #WM_event_consecutive_gesture_test check).
   * A #wmEventType is assigned to this value.
   */
  short event_queue_consecutive_gesture_type = 0;
  /** The cursor location when `event_queue_consecutive_gesture_type` was set. */
  int event_queue_consecutive_gesture_xy[2] = {};
  /** See #WM_event_consecutive_data_get and related API. Freed when consecutive events end. */
  struct wmEvent_ConsecutiveData *event_queue_consecutive_gesture_data = nullptr;

  /**
   * Internal: tag this for extra mouse-move event,
   * makes cursors/buttons active on UI switching.
   */
  char addmousemove = 0;
  char _pad1[7] = {};

  /** Properties for stereoscopic displays. */
  struct Stereo3dFormat *stereo3d_format = nullptr;

  bke::WindowRuntime *runtime = nullptr;
};

#ifdef ime_data
#  undef ime_data
#endif

/* These two lines with # tell `makesdna` this struct can be excluded. */
/* should be something like DNA_EXCLUDE
 * but the preprocessor first removes all comments, spaces etc */
#
#
struct wmOperatorTypeMacro {
  struct wmOperatorTypeMacro *next = nullptr, *prev = nullptr;

  /* operator id */
  char idname[/*OP_MAX_TYPENAME*/ 64] = "";
  /* rna pointer to access properties, like keymap */
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  struct IDProperty *properties = nullptr;
  struct PointerRNA *ptr = nullptr;
};

/**
 * Partial copy of the event, for matching by event handler.
 */
struct wmKeyMapItem {
  struct wmKeyMapItem *next = nullptr, *prev = nullptr;

  /* operator */
  /** Used to retrieve operator type pointer. */
  char idname[64] = "";
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  IDProperty *properties = nullptr;

  /* modal */
  /** Runtime temporary storage for loading. */
  char propvalue_str[64] = "";
  /** If used, the item is from modal map. */
  short propvalue = 0;

  /* event */
  /** Event code itself (#EVT_LEFTCTRLKEY, #LEFTMOUSE etc). */
  short type = 0;
  /** Button state (#KM_ANY, #KM_PRESS, #KM_DBL_CLICK, #KM_PRESS_DRAG, #KM_NOTHING etc). */
  int8_t val = 0;
  /**
   * The 2D direction of the event to use when `val == KM_PRESS_DRAG`.
   * Set to #KM_DIRECTION_N, #KM_DIRECTION_S & related values, #KM_NOTHING for any direction.
   */
  int8_t direction = 0;

  /* Modifier keys:
   * Valid values:
   * - #KM_ANY
   * - #KM_NOTHING
   * - #KM_MOD_HELD (not #KM_PRESS even though the values match).
   */

  int8_t shift = 0;
  int8_t ctrl = 0;
  int8_t alt = 0;
  /** Also known as "Apple", "Windows-Key" or "Super. */
  int8_t oskey = 0;
  /** See #KM_HYPER for details. */
  int8_t hyper = 0;

  char _pad0[7] = {};

  /** Raw-key modifier. */
  short keymodifier = 0;

  /* flag: inactive, expanded */
  uint8_t flag = 0;

  /* runtime */
  /** Keymap editor. */
  uint8_t maptype = 0;
  /** Unique identifier. Positive for kmi that override builtins, negative otherwise. */
  short id = 0;
  /**
   * RNA pointer to access properties.
   *
   * \note The `ptr.owner_id` value must be NULL, as a signal not to use the context
   * when running property callbacks such as ENUM item functions.
   */
  struct PointerRNA *ptr = nullptr;
};

/** Used instead of wmKeyMapItem for diff keymaps. */
struct wmKeyMapDiffItem {
  struct wmKeyMapDiffItem *next = nullptr, *prev = nullptr;

  wmKeyMapItem *remove_item = nullptr;
  wmKeyMapItem *add_item = nullptr;
};

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
 * Stored in WM, the actively used key-maps.
 */
struct wmKeyMap {
  struct wmKeyMap *next = nullptr, *prev = nullptr;

  ListBaseT<wmKeyMapItem> items = {nullptr, nullptr};
  ListBaseT<wmKeyMapDiffItem> diff_items = {nullptr, nullptr};

  /** Global editor keymaps, or for more per space/region. */
  char idname[64] = "";
  /** Same IDs as in DNA_space_types.h. */
  short spaceid = 0;
  /** See above. */
  short regionid = 0;
  /** Optional, see: #wmOwnerID. */
  char owner_id[128] = "";

  /** General flags. */
  short flag = 0;
  /** Last kmi id. */
  short kmi_id = 0;

  /* runtime */
  /** Verify if enabled in the current context, use #WM_keymap_poll instead of direct calls. */
  bool (*poll)(struct bContext *);
  bool (*poll_modal_item)(const struct wmOperator *op, int value) = {};

  /** For modal, #EnumPropertyItem for now. */
  const void *modal_items = nullptr;
};

/**
 * This is similar to addon-preferences,
 * however unlike add-ons key-configurations aren't saved to disk.
 *
 * #wmKeyConfigPref is written to DNA,
 * #wmKeyConfigPrefType_Runtime has the RNA type.
 */
struct wmKeyConfigPref {
  struct wmKeyConfigPref *next = nullptr, *prev = nullptr;
  /** Unique name. */
  char idname[64] = "";
  IDProperty *prop = nullptr;
};

/** #wmKeyConfig.flag */
enum {
  KEYCONF_USER = (1 << 1),         /* And what about (1 << 0)? */
  KEYCONF_INIT_DEFAULT = (1 << 2), /* Has default keymap been initialized? */
};

struct wmKeyConfig {
  struct wmKeyConfig *next = nullptr, *prev = nullptr;

  /** Unique name. */
  char idname[64] = "";
  /** ID-name of configuration this is derives from, "" if none. */
  char basename[64] = "";

  ListBaseT<wmKeyMap> keymaps = {nullptr, nullptr};
  int actkeymap = 0;
  short flag = 0;
  char _pad0[2] = {};
};

/**
 * This one is the operator itself, stored in files for macros etc.
 * operator + operator-type should be able to redo entirely, but for different context's.
 */
struct wmOperator {
  struct wmOperator *next = nullptr, *prev = nullptr;

  /* saved */
  /** Used to retrieve type pointer. */
  char idname[/*OP_MAX_TYPENAME*/ 64] = "";
  /** Saved, user-settable properties. */
  IDProperty *properties = nullptr;

  /* runtime */
  /** Operator type definition from idname. */
  struct wmOperatorType *type = nullptr;
  /** Custom storage, only while operator runs. */
  void *customdata = nullptr;
  /** Python stores the class instance here. */
  void *py_instance = nullptr;

  /** Rna pointer to access properties. */
  struct PointerRNA *ptr = nullptr;
  /** Errors and warnings storage. */
  struct ReportList *reports = nullptr;

  /** List of operators, can be a tree. */
  ListBaseT<wmOperator> macro = {nullptr, nullptr};
  /** Current running macro, not saved. */
  struct wmOperator *opm = nullptr;
  /** Runtime for drawing. */
  ui::Layout *layout = nullptr;
  short flag = 0;
  char _pad[6] = {};
};

}  // namespace blender
