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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_WINDOWMANAGER_TYPES_H__
#define __DNA_WINDOWMANAGER_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"

#include "DNA_ID.h"

/* defined here: */
struct wmWindow;
struct wmWindowManager;

struct wmEvent;
struct wmGesture;
struct wmKeyConfig;
struct wmKeyMap;
struct wmMsgBus;
struct wmOperator;
struct wmOperatorType;

/* forwards */
struct PointerRNA;
struct Report;
struct ReportList;
struct Stereo3dFormat;
struct UndoStep;
struct bContext;
struct bScreen;
struct uiLayout;
struct wmTimer;

#define OP_MAX_TYPENAME 64
#define KMAP_MAX_NAME 64

/* keep in sync with 'rna_enum_wm_report_items' in wm_rna.c */
typedef enum ReportType {
  RPT_DEBUG = (1 << 0),
  RPT_INFO = (1 << 1),
  RPT_OPERATOR = (1 << 2),
  RPT_PROPERTY = (1 << 3),
  RPT_WARNING = (1 << 4),
  RPT_ERROR = (1 << 5),
  RPT_ERROR_INVALID_INPUT = (1 << 6),
  RPT_ERROR_INVALID_CONTEXT = (1 << 7),
  RPT_ERROR_OUT_OF_MEMORY = (1 << 8),
} ReportType;

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
};

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct Report {
  struct Report *next, *prev;
  /** ReportType. */
  short type;
  short flag;
  /** `strlen(message)`, saves some time calculating the word wrap . */
  int len;
  const char *typestr;
  const char *message;
} Report;

/* saved in the wm, don't remove */
typedef struct ReportList {
  ListBase list;
  /** ReportType. */
  int printlevel;
  /** ReportType. */
  int storelevel;
  int flag;
  char _pad[4];
  struct wmTimer *reporttimer;
} ReportList;

/* timer customdata to control reports display */
/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct ReportTimerInfo {
  float col[4];
  float widthfac;
} ReportTimerInfo;

/* reports need to be before wmWindowManager */

/* windowmanager is saved, tag WMAN */
typedef struct wmWindowManager {
  ID id;

  /** Separate active from drawable. */
  struct wmWindow *windrawable, *winactive;
  ListBase windows;

  /** Set on file read. */
  int initialized;
  /** Indicator whether data was saved. */
  short file_saved;
  /** Operator stack depth to avoid nested undo pushes. */
  short op_undo_depth;

  /** Operator registry. */
  ListBase operators;

  /** Refresh/redraw wmNotifier structs. */
  ListBase queue;

  /** Information and error reports. */
  struct ReportList reports;

  /** Threaded jobs manager. */
  ListBase jobs;

  /** Extra overlay cursors to draw, like circles. */
  ListBase paintcursors;

  /** Active dragged items. */
  ListBase drags;

  /** Known key configurations. */
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

  /** Indicates whether interface is locked for user interaction. */
  char is_interface_locked;
  char par[7];

  struct wmMsgBus *message_bus;

} wmWindowManager;

/* wmWindowManager.initialized */
enum {
  WM_WINDOW_IS_INITIALIZED = (1 << 0),
  WM_KEYCONFIG_IS_INITIALIZED = (1 << 1),
};

#define WM_KEYCONFIG_STR_DEFAULT "blender"

/* IME is win32 only! */
#if !defined(WIN32) && !defined(DNA_DEPRECATED)
#  ifdef __GNUC__
#    define ime_data ime_data __attribute__((deprecated))
#  endif
#endif

/* the saveable part, rest of data is local in ghostwinlay */
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

  struct WorkSpaceInstanceHook *workspace_hook;

  /** Global areas aren't part of the screen, but part of the window directly.
   * \note Code assumes global areas with fixed height, fixed width not supported yet */
  ScrAreaMap global_areas;

  struct bScreen *screen DNA_DEPRECATED;

  /** Window coords. */
  short posx, posy, sizex, sizey;
  /** Borderless, full. */
  short windowstate;
  /** Multiscreen... no idea how to store yet. */
  short monitor;
  /** Set to 1 if an active window, for quick rejects. */
  short active;
  /** Current mouse cursor type. */
  short cursor;
  /** Previous cursor when setting modal one. */
  short lastcursor;
  /** The current modal cursor. */
  short modalcursor;
  /** Cursor grab mode. */
  short grabcursor;
  /** Internal: tag this for extra mousemove event,
   * makes cursors/buttons active on UI switching. */
  short addmousemove;

  /** Winid also in screens, is for retrieving this window after read. */
  int winid;

  /** Internal, lock pie creation from this event until released. */
  short lock_pie_event;
  /**
   * Exception to the above rule for nested pies, store last pie event for operators
   * that spawn a new pie right after destruction of last pie.
   */
  short last_pie_event;

  /** Storage for event system. */
  struct wmEvent *eventstate;

  /** Internal for wm_operators.c. */
  struct wmGesture *tweak;

  /* Input Method Editor data - complex character input (esp. for asian character input)
   * Currently WIN32, runtime-only data */
  struct wmIMEData *ime_data;

  /** All events (ghost level events were handled). */
  ListBase queue;
  /** Window+screen handlers, handled last. */
  ListBase handlers;
  /** Priority handlers, handled first. */
  ListBase modalhandlers;

  /** Gesture stuff. */
  ListBase gesture;

  /** Properties for stereoscopic displays. */
  struct Stereo3dFormat *stereo3d_format;

  /* custom drawing callbacks */
  ListBase drawcalls;

  /* Private runtime info to show text in the status bar. */
  void *cursor_keymap_status;
} wmWindow;

#ifdef ime_data
#  undef ime_data
#endif

/* These two Lines with # tell makesdna this struct can be excluded. */
/* should be something like DNA_EXCLUDE
 * but the preprocessor first removes all comments, spaces etc */
#
#
typedef struct wmOperatorTypeMacro {
  struct wmOperatorTypeMacro *next, *prev;

  /* operator id */
  char idname[64];
  /* rna pointer to access properties, like keymap */
  /** Operator properties, assigned to ptr->data and can be written to a file. */
  struct IDProperty *properties;
  struct PointerRNA *ptr;
} wmOperatorTypeMacro;

/* Partial copy of the event, for matching by event handler. */
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
  /** Event code itself. */
  short type;
  /** KM_ANY, KM_PRESS, KM_NOTHING etc. */
  short val;
  /** Oskey is apple or windowskey, value denotes order of pressed. */
  short shift, ctrl, alt, oskey;
  /** Rawkey modifier. */
  short keymodifier;

  /* flag: inactive, expanded */
  short flag;

  /* runtime */
  /** Keymap editor. */
  short maptype;
  /** Unique identifier. Positive for kmi that override builtins, negative otherwise. */
  short id;
  char _pad[2];
  /** Rna pointer to access properties. */
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
};

/** #wmKeyMapItem.maptype */
enum {
  KMI_TYPE_KEYBOARD = 0,
  KMI_TYPE_MOUSE = 1,
  KMI_TYPE_TWEAK = 2,
  KMI_TYPE_TEXTINPUT = 3,
  KMI_TYPE_TIMER = 4,
  KMI_TYPE_NDOF = 5,
};

/* stored in WM, the actively used keymaps */
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
  KEYMAP_MODAL = (1 << 0), /* modal map, not using operatornames */
  KEYMAP_USER = (1 << 1),  /* user keymap */
  KEYMAP_EXPANDED = (1 << 2),
  KEYMAP_CHILDREN_EXPANDED = (1 << 3),
  KEYMAP_DIFF = (1 << 4),          /* diff keymap for user preferences */
  KEYMAP_USER_MODIFIED = (1 << 5), /* keymap has user modifications */
  KEYMAP_UPDATE = (1 << 6),
  KEYMAP_TOOL = (1 << 7), /* keymap for active tool system */
};

/**
 * This is similar to addon-preferences,
 * however unlike add-ons key-config's aren't saved to disk.
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
  /** Idname of configuration this is derives from, "" if none. */
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
  char idname[64];
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
  /* add this flag if the event should pass through */
  OPERATOR_PASS_THROUGH = (1 << 3),
  /* in case operator got executed outside WM code... like via fileselect */
  OPERATOR_HANDLED = (1 << 4),
  /* used for operators that act indirectly (eg. popup menu)
   * note: this isn't great design (using operators to trigger UI) avoid where possible. */
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
  /** low level flag so exec() operators can tell if they were invoked, use with care.
   * Typically this shouldn't make any difference, but it rare cases its needed
   * (see smooth-view) */
  OP_IS_INVOKE = (1 << 0),
  /** So we can detect if an operators exec() call is activated by adjusting the last action. */
  OP_IS_REPEAT = (1 << 1),
  /**
   * So we can detect if an operators exec() call is activated from #SCREEN_OT_repeat_last.
   *
   * This difference can be important because previous settings may be used,
   * even with #PROP_SKIP_SAVE the repeat last operator will use the previous settings.
   * Unlike #OP_IS_REPEAT the selection (and context generally) may be be different each time.
   * See T60777 for an example of when this is needed.
   */
  OP_IS_REPEAT_LAST = (1 << 1),

  /** When the cursor is grabbed */
  OP_IS_MODAL_GRAB_CURSOR = (1 << 2),

  /** Allow modal operators to have the region under the cursor for their context
   * (the regiontype is maintained to prevent errors) */
  OP_IS_MODAL_CURSOR_REGION = (1 << 3),
};

#endif /* __DNA_WINDOWMANAGER_TYPES_H__ */
