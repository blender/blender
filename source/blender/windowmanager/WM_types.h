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
 * \ingroup wm
 *
 *
 * Overview of WM structs
 * ======================
 *
 * - #wmWindowManager.windows -> #wmWindow <br>
 *   Window manager stores a list of windows.
 *
 *   - #wmWindow.screen -> #bScreen <br>
 *     Window has an active screen.
 *
 *     - #bScreen.areabase -> #ScrArea <br>
 *       Link to #ScrArea.
 *
 *       - #ScrArea.spacedata <br>
 *         Stores multiple spaces via space links.
 *
 *         - #SpaceLink <br>
 *           Base struct for space data for all different space types.
 *
 *       - #ScrArea.regionbase -> #ARegion <br>
 *         Stores multiple regions.
 *
 *     - #bScreen.regionbase -> #ARegion <br>
 *       Global screen level regions, e.g. popups, popovers, menus.
 *
 *   - #wmWindow.global_areas -> #ScrAreaMap <br>
 *     Global screen via 'areabase', e.g. top-bar & status-bar.
 *
 *
 * Window Layout
 * =============
 *
 * <pre>
 * wmWindow -> bScreen
 * +----------------------------------------------------------+
 * |+-----------------------------------------+-------------+ |
 * ||ScrArea (links to 3D view)               |ScrArea      | |
 * ||+-------++----------+-------------------+|(links to    | |
 * |||ARegion||          |ARegion (quad view)|| properties) | |
 * |||(tools)||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       |+----------+-------------------+|             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * |||       ||          |                   ||             | |
 * ||+-------++----------+-------------------+|             | |
 * |+-----------------------------------------+-------------+ |
 * +----------------------------------------------------------+
 * </pre>
 *
 * Space Data
 * ==========
 *
 * <pre>
 * ScrArea's store a list of space data (SpaceLinks), each of unique type.
 * The first one is the displayed in the UI, others are added as needed.
 *
 * +----------------------------+  <-- area->spacedata.first;
 * |                            |
 * |                            |---+  <-- other inactive SpaceLink's stored.
 * |                            |   |
 * |                            |   |---+
 * |                            |   |   |
 * |                            |   |   |
 * |                            |   |   |
 * |                            |   |   |
 * +----------------------------+   |   |
 *    |                             |   |
 *    +-----------------------------+   |
 *       |                              |
 *       +------------------------------+
 * </pre>
 *
 * A common way to get the space from the ScrArea:
 * \code{.c}
 * if (area->spacetype == SPACE_VIEW3D) {
 *     View3D *v3d = area->spacedata.first;
 *     ...
 * }
 * \endcode
 */

#pragma once

struct ID;
struct ImBuf;
struct bContext;
struct wmEvent;
struct wmOperator;
struct wmWindowManager;

#include "BLI_compiler_attrs.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "RNA_types.h"

/* exported types for WM */
#include "gizmo/WM_gizmo_types.h"
#include "wm_cursors.h"
#include "wm_event_types.h"

/* Include external gizmo API's */
#include "gizmo/WM_gizmo_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wmGenericUserData {
  void *data;
  /** When NULL, use #MEM_freeN. */
  void (*free_fn)(void *data);
  bool use_free;
} wmGenericUserData;

typedef struct wmGenericCallback {
  void (*exec)(struct bContext *C, void *user_data);
  void *user_data;
  void (*free_user_data)(void *user_data);
} wmGenericCallback;

/* ************** wmOperatorType ************************ */

/** #wmOperatorType.flag */
enum {
  /** Register operators in stack after finishing (needed for redo). */
  OPTYPE_REGISTER = (1 << 0),
  /** Do an undo push after the operator runs. */
  OPTYPE_UNDO = (1 << 1),
  /** Let Blender grab all input from the WM (X11). */
  OPTYPE_BLOCKING = (1 << 2),
  OPTYPE_MACRO = (1 << 3),

  /** Grabs the cursor and optionally enables continuous cursor wrapping. */
  OPTYPE_GRAB_CURSOR_XY = (1 << 4),
  /** Only warp on the X axis. */
  OPTYPE_GRAB_CURSOR_X = (1 << 5),
  /** Only warp on the Y axis. */
  OPTYPE_GRAB_CURSOR_Y = (1 << 6),

  /** Show preset menu. */
  OPTYPE_PRESET = (1 << 7),

  /**
   * Some operators are mainly for internal use and don't make sense
   * to be accessed from the search menu, even if poll() returns true.
   * Currently only used for the search toolbox.
   */
  OPTYPE_INTERNAL = (1 << 8),

  /** Allow operator to run when interface is locked. */
  OPTYPE_LOCK_BYPASS = (1 << 9),
  /** Special type of undo which doesn't store itself multiple times. */
  OPTYPE_UNDO_GROUPED = (1 << 10),
};

/** For #WM_cursor_grab_enable wrap axis. */
enum {
  WM_CURSOR_WRAP_NONE = 0,
  WM_CURSOR_WRAP_X,
  WM_CURSOR_WRAP_Y,
  WM_CURSOR_WRAP_XY,
};

/**
 * Context to call operator in for #WM_operator_name_call.
 * rna_ui.c contains EnumPropertyItem's of these, keep in sync.
 */
enum {
  /* if there's invoke, call it, otherwise exec */
  WM_OP_INVOKE_DEFAULT,
  WM_OP_INVOKE_REGION_WIN,
  WM_OP_INVOKE_REGION_CHANNELS,
  WM_OP_INVOKE_REGION_PREVIEW,
  WM_OP_INVOKE_AREA,
  WM_OP_INVOKE_SCREEN,
  /* only call exec */
  WM_OP_EXEC_DEFAULT,
  WM_OP_EXEC_REGION_WIN,
  WM_OP_EXEC_REGION_CHANNELS,
  WM_OP_EXEC_REGION_PREVIEW,
  WM_OP_EXEC_AREA,
  WM_OP_EXEC_SCREEN,
};

/* property tags for RNA_OperatorProperties */
typedef enum eOperatorPropTags {
  OP_PROP_TAG_ADVANCED = (1 << 0),
} eOperatorPropTags;
#define OP_PROP_TAG_ADVANCED ((eOperatorPropTags)OP_PROP_TAG_ADVANCED)

/* ************** wmKeyMap ************************ */

/* modifier */
#define KM_SHIFT 1
#define KM_CTRL 2
#define KM_ALT 4
#define KM_OSKEY 8
/* means modifier should be pressed 2nd */
#define KM_SHIFT2 16
#define KM_CTRL2 32
#define KM_ALT2 64
#define KM_OSKEY2 128

/* KM_MOD_ flags for wmKeyMapItem and wmEvent.alt/shift/oskey/ctrl  */
/* note that KM_ANY and KM_NOTHING are used with these defines too */
#define KM_MOD_FIRST 1
#define KM_MOD_SECOND 2

/* type: defined in wm_event_types.c */
#define KM_TEXTINPUT -2

/* val */
#define KM_ANY -1
#define KM_NOTHING 0
#define KM_PRESS 1
#define KM_RELEASE 2
#define KM_CLICK 3
#define KM_DBL_CLICK 4
#define KM_CLICK_DRAG 5

/* ************** UI Handler ***************** */

#define WM_UI_HANDLER_CONTINUE 0
#define WM_UI_HANDLER_BREAK 1

/* ************** Notifiers ****************** */

typedef struct wmNotifier {
  struct wmNotifier *next, *prev;

  const struct wmWindow *window;

  unsigned int category, data, subtype, action;

  void *reference;

} wmNotifier;

/* 4 levels
 *
 * 0xFF000000; category
 * 0x00FF0000; data
 * 0x0000FF00; data subtype (unused?)
 * 0x000000FF; action
 */

/* category */
#define NOTE_CATEGORY 0xFF000000
#define NC_WM (1 << 24)
#define NC_WINDOW (2 << 24)
#define NC_SCREEN (3 << 24)
#define NC_SCENE (4 << 24)
#define NC_OBJECT (5 << 24)
#define NC_MATERIAL (6 << 24)
#define NC_TEXTURE (7 << 24)
#define NC_LAMP (8 << 24)
#define NC_GROUP (9 << 24)
#define NC_IMAGE (10 << 24)
#define NC_BRUSH (11 << 24)
#define NC_TEXT (12 << 24)
#define NC_WORLD (13 << 24)
#define NC_ANIMATION (14 << 24)
/* When passing a space as reference data with this (e.g. `WM_event_add_notifier(..., space)`),
 * the notifier will only be sent to this space. That avoids unnecessary updates for unrelated
 * spaces. */
#define NC_SPACE (15 << 24)
#define NC_GEOM (16 << 24)
#define NC_NODE (17 << 24)
#define NC_ID (18 << 24)
#define NC_PAINTCURVE (19 << 24)
#define NC_MOVIECLIP (20 << 24)
#define NC_MASK (21 << 24)
#define NC_GPENCIL (22 << 24)
#define NC_LINESTYLE (23 << 24)
#define NC_CAMERA (24 << 24)
#define NC_LIGHTPROBE (25 << 24)
/* Changes to asset data in the current .blend. */
#define NC_ASSET (26 << 24)

/* data type, 256 entries is enough, it can overlap */
#define NOTE_DATA 0x00FF0000

/* NC_WM windowmanager */
#define ND_FILEREAD (1 << 16)
#define ND_FILESAVE (2 << 16)
#define ND_DATACHANGED (3 << 16)
#define ND_HISTORY (4 << 16)
#define ND_JOB (5 << 16)
#define ND_UNDO (6 << 16)
#define ND_XR_DATA_CHANGED (7 << 16)
#define ND_LIB_OVERRIDE_CHANGED (8 << 16)

/* NC_SCREEN */
#define ND_LAYOUTBROWSE (1 << 16)
#define ND_LAYOUTDELETE (2 << 16)
#define ND_ANIMPLAY (4 << 16)
#define ND_GPENCIL (5 << 16)
#define ND_EDITOR_CHANGED (6 << 16) /*sent to new editors after switching to them*/
#define ND_LAYOUTSET (7 << 16)
#define ND_SKETCH (8 << 16)
#define ND_WORKSPACE_SET (9 << 16)
#define ND_WORKSPACE_DELETE (10 << 16)

/* NC_SCENE Scene */
#define ND_SCENEBROWSE (1 << 16)
#define ND_MARKERS (2 << 16)
#define ND_FRAME (3 << 16)
#define ND_RENDER_OPTIONS (4 << 16)
#define ND_NODES (5 << 16)
#define ND_SEQUENCER (6 << 16)
/* Note: If an object was added, removed, merged/joined, ..., it is not enough to notify with
 * this. This affects the layer so also send a layer change notifier (e.g. ND_LAYER_CONTENT)! */
#define ND_OB_ACTIVE (7 << 16)
/* See comment on ND_OB_ACTIVE. */
#define ND_OB_SELECT (8 << 16)
#define ND_OB_VISIBLE (9 << 16)
#define ND_OB_RENDER (10 << 16)
#define ND_MODE (11 << 16)
#define ND_RENDER_RESULT (12 << 16)
#define ND_COMPO_RESULT (13 << 16)
#define ND_KEYINGSET (14 << 16)
#define ND_TOOLSETTINGS (15 << 16)
#define ND_LAYER (16 << 16)
#define ND_FRAME_RANGE (17 << 16)
#define ND_TRANSFORM_DONE (18 << 16)
#define ND_WORLD (92 << 16)
#define ND_LAYER_CONTENT (101 << 16)

/* NC_OBJECT Object */
#define ND_TRANSFORM (18 << 16)
#define ND_OB_SHADING (19 << 16)
#define ND_POSE (20 << 16)
#define ND_BONE_ACTIVE (21 << 16)
#define ND_BONE_SELECT (22 << 16)
#define ND_DRAW (23 << 16)
#define ND_MODIFIER (24 << 16)
#define ND_KEYS (25 << 16)
#define ND_CONSTRAINT (26 << 16)
#define ND_PARTICLE (27 << 16)
#define ND_POINTCACHE (28 << 16)
#define ND_PARENT (29 << 16)
#define ND_LOD (30 << 16)
#define ND_DRAW_RENDER_VIEWPORT \
  (31 << 16) /* for camera & sequencer viewport update, also /w NC_SCENE */
#define ND_SHADERFX (32 << 16)

/* NC_MATERIAL Material */
#define ND_SHADING (30 << 16)
#define ND_SHADING_DRAW (31 << 16)
#define ND_SHADING_LINKS (32 << 16)
#define ND_SHADING_PREVIEW (33 << 16)

/* NC_LAMP Light */
#define ND_LIGHTING (40 << 16)
#define ND_LIGHTING_DRAW (41 << 16)

/* NC_WORLD World */
#define ND_WORLD_DRAW (45 << 16)

/* NC_TEXT Text */
#define ND_CURSOR (50 << 16)
#define ND_DISPLAY (51 << 16)

/* NC_ANIMATION Animato */
#define ND_KEYFRAME (70 << 16)
#define ND_KEYFRAME_PROP (71 << 16)
#define ND_ANIMCHAN (72 << 16)
#define ND_NLA (73 << 16)
#define ND_NLA_ACTCHANGE (74 << 16)
#define ND_FCURVES_ORDER (75 << 16)
#define ND_NLA_ORDER (76 << 16)

/* NC_GPENCIL */
#define ND_GPENCIL_EDITMODE (85 << 16)

/* NC_GEOM Geometry */
/* Mesh, Curve, MetaBall, Armature, .. */
#define ND_SELECT (90 << 16)
#define ND_DATA (91 << 16)
#define ND_VERTEX_GROUP (92 << 16)

/* NC_NODE Nodes */

/* NC_SPACE */
#define ND_SPACE_CONSOLE (1 << 16)     /* general redraw */
#define ND_SPACE_INFO_REPORT (2 << 16) /* update for reports, could specify type */
#define ND_SPACE_INFO (3 << 16)
#define ND_SPACE_IMAGE (4 << 16)
#define ND_SPACE_FILE_PARAMS (5 << 16)
#define ND_SPACE_FILE_LIST (6 << 16)
#define ND_SPACE_ASSET_PARAMS (7 << 16)
#define ND_SPACE_NODE (8 << 16)
#define ND_SPACE_OUTLINER (9 << 16)
#define ND_SPACE_VIEW3D (10 << 16)
#define ND_SPACE_PROPERTIES (11 << 16)
#define ND_SPACE_TEXT (12 << 16)
#define ND_SPACE_TIME (13 << 16)
#define ND_SPACE_GRAPH (14 << 16)
#define ND_SPACE_DOPESHEET (15 << 16)
#define ND_SPACE_NLA (16 << 16)
#define ND_SPACE_SEQUENCER (17 << 16)
#define ND_SPACE_NODE_VIEW (18 << 16)
#define ND_SPACE_CHANGED (19 << 16) /*sent to a new editor type after it's replaced an old one*/
#define ND_SPACE_CLIP (20 << 16)
#define ND_SPACE_FILE_PREVIEW (21 << 16)
#define ND_SPACE_SPREADSHEET (22 << 16)

/* subtype, 256 entries too */
#define NOTE_SUBTYPE 0x0000FF00

/* subtype scene mode */
#define NS_MODE_OBJECT (1 << 8)

#define NS_EDITMODE_MESH (2 << 8)
#define NS_EDITMODE_CURVE (3 << 8)
#define NS_EDITMODE_SURFACE (4 << 8)
#define NS_EDITMODE_TEXT (5 << 8)
#define NS_EDITMODE_MBALL (6 << 8)
#define NS_EDITMODE_LATTICE (7 << 8)
#define NS_EDITMODE_ARMATURE (8 << 8)
#define NS_MODE_POSE (9 << 8)
#define NS_MODE_PARTICLE (10 << 8)

/* subtype 3d view editing */
#define NS_VIEW3D_GPU (16 << 8)
#define NS_VIEW3D_SHADING (17 << 8)

/* subtype layer editing */
#define NS_LAYER_COLLECTION (24 << 8)

/* action classification */
#define NOTE_ACTION (0x000000FF)
#define NA_EDITED 1
#define NA_EVALUATED 2
#define NA_ADDED 3
#define NA_REMOVED 4
#define NA_RENAME 5
#define NA_SELECTED 6
#define NA_ACTIVATED 7
#define NA_PAINTING 8
#define NA_JOB_FINISHED 9

/* ************** Gesture Manager data ************** */

/* wmGesture->type */
#define WM_GESTURE_TWEAK 0
#define WM_GESTURE_LINES 1
#define WM_GESTURE_RECT 2
#define WM_GESTURE_CROSS_RECT 3
#define WM_GESTURE_LASSO 4
#define WM_GESTURE_CIRCLE 5
#define WM_GESTURE_STRAIGHTLINE 6

/**
 * wmGesture is registered to #wmWindow.gesture, handled by operator callbacks.
 * Tweak gesture is builtin feature.
 */
typedef struct wmGesture {
  struct wmGesture *next, *prev;
  /** #wmEvent.type */
  int event_type;
  /** Gesture type define. */
  int type;
  /** bounds of region to draw gesture within. */
  rcti winrct;
  /** optional, amount of points stored. */
  int points;
  /** optional, maximum amount of points stored. */
  int points_alloc;
  int modal_state;
  /** optional, draw the active side of the straightline gesture. */
  bool draw_active_side;

  /**
   * For modal operators which may be running idle, waiting for an event to activate the gesture.
   * Typically this is set when the user is click-dragging the gesture
   * (box and circle select for eg).
   */
  uint is_active : 1;
  /** Previous value of is-active (use to detect first run & edge cases). */
  uint is_active_prev : 1;
  /** Use for gestures that support both immediate or delayed activation. */
  uint wait_for_input : 1;
  /** Use for gestures that can be moved, like box selection */
  uint move : 1;
  /** For gestures that support snapping, stores if snapping is enabled using the modal keymap
   * toggle. */
  uint use_snap : 1;
  /** For gestures that support flip, stores if flip is enabled using the modal keymap
   * toggle. */
  uint use_flip : 1;

  /**
   * customdata
   * - for border is a #rcti.
   * - for circle is recti, (xmin, ymin) is center, xmax radius.
   * - for lasso is short array.
   * - for straight line is a recti: (xmin,ymin) is start, (xmax, ymax) is end.
   */
  void *customdata;

  /** Free pointer to use for operator allocs (if set, its freed on exit). */
  wmGenericUserData user_data;
} wmGesture;

/* ************** wmEvent ************************ */

typedef struct wmTabletData {
  /** 0=EVT_TABLET_NONE, 1=EVT_TABLET_STYLUS, 2=EVT_TABLET_ERASER. */
  int active;
  /** range 0.0 (not touching) to 1.0 (full pressure). */
  float pressure;
  /** range 0.0 (upright) to 1.0 (tilted fully against the tablet surface). */
  float x_tilt;
  /** as above. */
  float y_tilt;
  /** Interpret mouse motion as absolute as typical for tablets. */
  char is_motion_absolute;
} wmTabletData;

/**
 * Each event should have full modifier state.
 * event comes from event manager and from keymap.
 *
 *
 * Previous State
 * ==============
 *
 * Events hold information about the previous event,
 * this is used for detecting click and double-click events (the timer is needed for double-click).
 * See #wm_event_add_ghostevent for implementation details.
 *
 * Notes:
 *
 * - The previous values are only set for mouse button and keyboard events.
 *   See: #ISMOUSE_BUTTON & #ISKEYBOARD macros.
 *
 * - Previous x/y are exceptions: #wmEvent.prevx & #wmEvent.prevy
 *   these are set on mouse motion, see #MOUSEMOVE & track-pad events.
 *
 * - Modal key-map handling sets `prevval` & `prevtype` to `val` & `type`,
 *   this allows modal keys-maps to check the original values (needed in some cases).
 */
typedef struct wmEvent {
  struct wmEvent *next, *prev;

  /** Event code itself (short, is also in key-map). */
  short type;
  /** Press, release, scroll-value. */
  short val;
  /** Mouse pointer position, screen coord. */
  int x, y;
  /** Region relative mouse position (name convention before Blender 2.5). */
  int mval[2];
  /**
   * From, ghost if utf8 is enabled for the platform,
   * #BLI_str_utf8_size() must _always_ be valid, check
   * when assigning s we don't need to check on every access after.
   */
  char utf8_buf[6];
  /** From ghost, fallback if utf8 isn't set. */
  char ascii;

  /**
   * Generated by auto-repeat, note that this must only ever be set for keyboard events
   * where `ISKEYBOARD(event->type) == true`.
   *
   * See #KMI_REPEAT_IGNORE for details on how key-map handling uses this.
   */
  char is_repeat;

  /** The previous value of `type`. */
  short prevtype;
  /** The previous value of `val`. */
  short prevval;
  /** The time when the key is pressed, see #PIL_check_seconds_timer. */
  double prevclicktime;
  /** The location when the key is pressed (used to enforce drag thresholds). */
  int prevclickx, prevclicky;
  /**
   * The previous value of #wmEvent.x #wmEvent.y,
   * Unlike other previous state variables, this is set on any mouse motion.
   * Use `prevclickx` & `prevclicky` for the value at time of pressing.
   */
  int prevx, prevy;

  /** Modifier states. */
  /** 'oskey' is apple or windows-key, value denotes order of pressed. */
  short shift, ctrl, alt, oskey;
  /** Raw-key modifier (allow using any key as a modifier). */
  short keymodifier;

  /** Tablet info, available for mouse move and button events. */
  wmTabletData tablet;

  /* Custom data. */
  /** Custom data type, stylus, 6dof, see wm_event_types.h */
  short custom;
  short customdatafree;
  int pad2;
  /** Ascii, unicode, mouse-coords, angles, vectors, NDOF data, drag-drop info. */
  void *customdata;

  /**
   * True if the operating system inverted the delta x/y values and resulting
   * `prevx`, `prevy` values, for natural scroll direction.
   * For absolute scroll direction, the delta must be negated again.
   */
  char is_direction_inverted;
} wmEvent;

/**
 * Values below are ignored when detecting if the user intentionally moved the cursor.
 * Keep this very small since it's used for selection cycling for eg,
 * where we want intended adjustments to pass this threshold and select new items.
 *
 * Always check for <= this value since it may be zero.
 */
#define WM_EVENT_CURSOR_MOTION_THRESHOLD ((float)U.move_threshold * U.dpi_fac)

/** Motion progress, for modal handlers. */
typedef enum {
  P_NOT_STARTED,
  P_STARTING,    /* <-- */
  P_IN_PROGRESS, /* <-- only these are sent for NDOF motion. */
  P_FINISHING,   /* <-- */
  P_FINISHED,
} wmProgress;

#ifdef WITH_INPUT_NDOF
typedef struct wmNDOFMotionData {
  /* awfully similar to GHOST_TEventNDOFMotionData... */
  /**
   * Each component normally ranges from -1 to +1, but can exceed that.
   * These use blender standard view coordinates,
   * with positive rotations being CCW about the axis.
   */
  /** Translation. */
  float tvec[3];
  /** Rotation.
   * <pre>
   * axis = (rx,ry,rz).normalized.
   * amount = (rx,ry,rz).magnitude [in revolutions, 1.0 = 360 deg]
   * </pre>
   */
  float rvec[3];
  /** Time since previous NDOF Motion event. */
  float dt;
  /** Is this the first event, the last, or one of many in between? */
  wmProgress progress;
} wmNDOFMotionData;
#endif /* WITH_INPUT_NDOF */

/** Timer flags. */
typedef enum {
  /** Do not attempt to free customdata pointer even if non-NULL. */
  WM_TIMER_NO_FREE_CUSTOM_DATA = 1 << 0,
} wmTimerFlags;

typedef struct wmTimer {
  struct wmTimer *next, *prev;

  /** Window this timer is attached to (optional). */
  struct wmWindow *win;

  /** Set by timer user. */
  double timestep;
  /** Set by timer user, goes to event system. */
  int event_type;
  /** Various flags controlling timer options, see below. */
  wmTimerFlags flags;
  /** Set by timer user, to allow custom values. */
  void *customdata;

  /** Total running time in seconds. */
  double duration;
  /** Time since previous step in seconds. */
  double delta;

  /** Internal, last time timer was activated. */
  double ltime;
  /** Internal, next time we want to activate the timer. */
  double ntime;
  /** Internal, when the timer started. */
  double stime;
  /** Internal, put timers to sleep when needed. */
  bool sleep;
} wmTimer;

typedef struct wmOperatorType {
  /** Text for UI, undo. */
  const char *name;
  /** Unique identifier. */
  const char *idname;
  const char *translation_context;
  /** Use for tool-tips and Python docs. */
  const char *description;
  /** Identifier to group operators together. */
  const char *undo_group;

  /**
   * This callback executes the operator without any interactive input,
   * parameters may be provided through operator properties. cannot use
   * any interface code or input device state.
   * See defines below for return values.
   */
  int (*exec)(struct bContext *, struct wmOperator *) ATTR_WARN_UNUSED_RESULT;

  /**
   * This callback executes on a running operator whenever as property
   * is changed. It can correct its own properties or report errors for
   * invalid settings in exceptional cases.
   * Boolean return value, True denotes a change has been made and to redraw.
   */
  bool (*check)(struct bContext *, struct wmOperator *);

  /**
   * For modal temporary operators, initially invoke is called. then
   * any further events are handled in modal. if the operation is
   * canceled due to some external reason, cancel is called
   * See defines below for return values.
   */
  int (*invoke)(struct bContext *,
                struct wmOperator *,
                const struct wmEvent *) ATTR_WARN_UNUSED_RESULT;

  /**
   * Called when a modal operator is canceled (not used often).
   * Internal cleanup can be done here if needed.
   */
  void (*cancel)(struct bContext *, struct wmOperator *);

  /**
   * Modal is used for operators which continuously run, eg:
   * fly mode, knife tool, circle select are all examples of modal operators.
   * Modal operators can handle events which would normally access other operators,
   * they keep running until they don't return `OPERATOR_RUNNING_MODAL`.
   */
  int (*modal)(struct bContext *,
               struct wmOperator *,
               const struct wmEvent *) ATTR_WARN_UNUSED_RESULT;

  /**
   * Verify if the operator can be executed in the current context, note
   * that the operator might still fail to execute even if this return true.
   */
  bool (*poll)(struct bContext *) ATTR_WARN_UNUSED_RESULT;

  /**
   * Use to check if properties should be displayed in auto-generated UI.
   * Use 'check' callback to enforce refreshing.
   */
  bool (*poll_property)(const struct bContext *C,
                        struct wmOperator *op,
                        const PropertyRNA *prop) ATTR_WARN_UNUSED_RESULT;

  /** Optional panel for redo and repeat, auto-generated if not set. */
  void (*ui)(struct bContext *, struct wmOperator *);

  /**
   * Return a different name to use in the user interface, based on property values.
   * The returned string does not need to be freed.
   */
  const char *(*get_name)(struct wmOperatorType *, struct PointerRNA *);

  /**
   * Return a different description to use in the user interface, based on property values.
   * The returned string must be freed by the caller, unless NULL.
   */
  char *(*get_description)(struct bContext *C, struct wmOperatorType *, struct PointerRNA *);

  /** rna for properties */
  struct StructRNA *srna;

  /** previous settings - for initializing on re-use */
  struct IDProperty *last_properties;

  /**
   * Default rna property to use for generic invoke functions.
   * menus, enum search... etc. Example: Enum 'type' for a Delete menu.
   *
   * When assigned a string/number property,
   * immediately edit the value when used in a popup. see: #UI_BUT_ACTIVATE_ON_INIT.
   */
  PropertyRNA *prop;

  /** struct wmOperatorTypeMacro */
  ListBase macro;

  /** pointer to modal keymap, do not free! */
  struct wmKeyMap *modalkeymap;

  /** python needs the operator type as well */
  bool (*pyop_poll)(struct bContext *, struct wmOperatorType *ot) ATTR_WARN_UNUSED_RESULT;

  /** RNA integration */
  ExtensionRNA rna_ext;

  /** Flag last for padding */
  short flag;

} wmOperatorType;

/**
 * Wrapper to reference a #wmOperatorType together with some set properties and other relevant
 * information to invoke the operator in a customizable way.
 */
typedef struct wmOperatorCallParams {
  struct wmOperatorType *optype;
  struct PointerRNA *opptr;
  short opcontext;
} wmOperatorCallParams;

#ifdef WITH_INPUT_IME
/* *********** Input Method Editor (IME) *********** */
/**
 * \note similar to #GHOST_TEventImeData.
 */
typedef struct wmIMEData {
  size_t result_len, composite_len;

  /** utf8 encoding */
  char *str_result;
  /** utf8 encoding */
  char *str_composite;

  /** Cursor position in the IME composition. */
  int cursor_pos;
  /** Beginning of the selection. */
  int sel_start;
  /** End of the selection. */
  int sel_end;

  bool is_ime_composing;
} wmIMEData;
#endif

/* **************** Paint Cursor ******************* */

typedef void (*wmPaintCursorDraw)(struct bContext *C, int, int, void *customdata);

/* *************** Drag and drop *************** */

#define WM_DRAG_ID 0
#define WM_DRAG_ASSET 1
#define WM_DRAG_RNA 2
#define WM_DRAG_PATH 3
#define WM_DRAG_NAME 4
#define WM_DRAG_VALUE 5
#define WM_DRAG_COLOR 6
#define WM_DRAG_DATASTACK 7

typedef enum wmDragFlags {
  WM_DRAG_NOP = 0,
  WM_DRAG_FREE_DATA = 1,
} wmDragFlags;

/* note: structs need not exported? */

typedef struct wmDragID {
  struct wmDragID *next, *prev;
  struct ID *id;
  struct ID *from_parent;
} wmDragID;

typedef struct wmDragAsset {
  char name[64]; /* MAX_NAME */
  /* Always freed. */
  const char *path;
  int id_type;
} wmDragAsset;

typedef struct wmDrag {
  struct wmDrag *next, *prev;

  int icon;
  /** See 'WM_DRAG_' defines above. */
  int type;
  void *poin;
  char path[1024]; /* FILE_MAX */
  double value;

  /** If no icon but imbuf should be drawn around cursor. */
  struct ImBuf *imb;
  float scale;
  int sx, sy;

  /** If set, draws operator name. */
  char opname[200];
  unsigned int flags;

  /** List of wmDragIDs, all are guaranteed to have the same ID type. */
  ListBase ids;
} wmDrag;

/**
 * Dropboxes are like keymaps, part of the screen/area/region definition.
 * Allocation and free is on startup and exit.
 */
typedef struct wmDropBox {
  struct wmDropBox *next, *prev;

  /** Test if the dropbox is active, then can print optype name. */
  bool (*poll)(struct bContext *, struct wmDrag *, const wmEvent *, const char **);

  /** Before exec, this copies drag info to #wmDrop properties. */
  void (*copy)(struct wmDrag *, struct wmDropBox *);

  /**
   * If the operator is cancelled (returns `OPERATOR_CANCELLED`), this can be used for cleanup of
   * `copy()` resources.
   */
  void (*cancel)(struct Main *, struct wmDrag *, struct wmDropBox *);

  /**
   * If poll succeeds, operator is called.
   * Not saved in file, so can be pointer.
   */
  wmOperatorType *ot;

  /** Operator properties, assigned to ptr->data and can be written to a file. */
  struct IDProperty *properties;
  /** RNA pointer to access properties. */
  struct PointerRNA *ptr;

  /** Default invoke. */
  short opcontext;

} wmDropBox;

/**
 * Struct to store tool-tip timer and possible creation if the time is reached.
 * Allows UI code to call #WM_tooltip_timer_init without each user having to handle the timer.
 */
typedef struct wmTooltipState {
  /** Create tooltip on this event. */
  struct wmTimer *timer;
  /** The area the tooltip is created in. */
  struct ScrArea *area_from;
  /** The region the tooltip is created in. */
  struct ARegion *region_from;
  /** The tooltip region. */
  struct ARegion *region;
  /** Create the tooltip region (assign to 'region'). */
  struct ARegion *(*init)(struct bContext *C,
                          struct ARegion *region,
                          int *pass,
                          double *pass_delay,
                          bool *r_exit_on_event);
  /** Exit on any event, not needed for buttons since their highlight state is used. */
  bool exit_on_event;
  /** Cursor location at the point of tooltip creation. */
  int event_xy[2];
  /** Pass, use when we want multiple tips, count down to zero. */
  int pass;
} wmTooltipState;

/* *************** migrated stuff, clean later? ************** */

typedef struct RecentFile {
  struct RecentFile *next, *prev;
  char *filepath;
} RecentFile;

/* Logging */
struct CLG_LogRef;
/* wm_init_exit.c */
extern struct CLG_LogRef *WM_LOG_OPERATORS;
extern struct CLG_LogRef *WM_LOG_HANDLERS;
extern struct CLG_LogRef *WM_LOG_EVENTS;
extern struct CLG_LogRef *WM_LOG_KEYMAPS;
extern struct CLG_LogRef *WM_LOG_TOOLS;
extern struct CLG_LogRef *WM_LOG_MSGBUS_PUB;
extern struct CLG_LogRef *WM_LOG_MSGBUS_SUB;

#ifdef __cplusplus
}
#endif
