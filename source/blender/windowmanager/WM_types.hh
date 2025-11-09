/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 *     Global screen via `areabase`, e.g. top-bar & status-bar.
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
struct bContextStore;
struct GreasePencil;
struct GreasePencilLayerTreeNode;
struct ReportList;
struct wmDrag;
struct wmDropBox;
struct wmEvent;
struct wmOperator;
struct wmWindowManager;

#include <memory>
#include <string>

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_vector.hh"

#include "DNA_listBase.h"
#include "DNA_uuid_types.h"
#include "DNA_vec_types.h"
#include "DNA_xr_types.h"

#include "BKE_wm_runtime.hh"  // IWYU pragma: export

#include "RNA_types.hh"

/* Exported types for WM. */
#include "gizmo/WM_gizmo_types.hh"  // IWYU pragma: export
#include "wm_cursors.hh"            // IWYU pragma: export
#include "wm_event_types.hh"        // IWYU pragma: export

/* Include external gizmo API's. */
#include "gizmo/WM_gizmo_api.hh"  // IWYU pragma: export

namespace blender::asset_system {
class AssetRepresentation;
}
using AssetRepresentationHandle = blender::asset_system::AssetRepresentation;

using wmGenericUserDataFreeFn = void (*)(void *data);

struct wmGenericUserData {
  void *data;
  /** When NULL, use #MEM_freeN. */
  wmGenericUserDataFreeFn free_fn;
  bool use_free;
};

using wmGenericCallbackFn = void (*)(bContext *C, void *user_data);

struct wmGenericCallback {
  wmGenericCallbackFn exec;
  void *user_data;
  wmGenericUserDataFreeFn free_user_data;
};

/* ************** wmOperatorType ************************ */

/** #wmOperatorType.flag */
enum {
  /**
   * Register operators in stack after finishing (needed for redo).
   *
   * \note Typically this flag should be enabled along with #OPTYPE_UNDO.
   * There are some exceptions to this:
   *
   * - Operators can conditionally perform an undo push,
   *   Examples include operators that may modify "screen" data
   *   (which the undo system doesn't track), or data-blocks such as objects, meshes etc.
   *   In this case the undo push depends on the operators internal logic.
   *
   *   We could support this as part of the operator return flag,
   *   currently it requires explicit calls to undo push.
   *
   * - Operators can perform an undo push indirectly.
   *   (`UI_OT_reset_default_button` for example).
   *
   *   In this case, register needs to be enabled so as not to clear the "Redo" panel, see #133761.
   *   Unless otherwise stated, any operators that register without the undo flag
   *   can be assumed to be creating undo steps indirectly (potentially at least).
   */
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

  /**
   * Depends on the cursor location, when activated from a menu wait for mouse press.
   *
   * In practice these operators often end up being accessed:
   * - Directly from key bindings.
   * - As tools in the toolbar.
   *
   * Even so, accessing from the menu should behave usefully.
   */
  OPTYPE_DEPENDS_ON_CURSOR = (1 << 11),

  /** Handle events before modal operators without this flag. */
  OPTYPE_MODAL_PRIORITY = (1 << 12),
};

/** For #WM_cursor_grab_enable wrap axis. */
enum eWM_CursorWrapAxis {
  WM_CURSOR_WRAP_NONE = 0,
  WM_CURSOR_WRAP_X,
  WM_CURSOR_WRAP_Y,
  WM_CURSOR_WRAP_XY,
};

/**
 * Context to call operator in for #WM_operator_name_call.
 * rna_ui.cc contains EnumPropertyItem's of these, keep in sync.
 */
namespace blender::wm {
enum class OpCallContext : int8_t {
  /* If there's invoke, call it, otherwise exec. */
  InvokeDefault,
  InvokeRegionWin,
  InvokeRegionChannels,
  InvokeRegionPreview,
  InvokeArea,
  InvokeScreen,
  /* Only call exec. */
  ExecDefault,
  ExecRegionWin,
  ExecRegionChannels,
  ExecRegionPreview,
  ExecArea,
  ExecScreen,
};
}

#define WM_OP_CONTEXT_HAS_AREA(type) \
  (CHECK_TYPE_INLINE(type, blender::wm::OpCallContext), \
   !ELEM(type, blender::wm::OpCallContext::InvokeScreen, blender::wm::OpCallContext::ExecScreen))
#define WM_OP_CONTEXT_HAS_REGION(type) \
  (WM_OP_CONTEXT_HAS_AREA(type) && \
   !ELEM(type, blender::wm::OpCallContext::InvokeArea, blender::wm::OpCallContext::ExecArea))

/** Property tags for #RNA_OperatorProperties. */
enum eOperatorPropTags {
  OP_PROP_TAG_ADVANCED = (1 << 0),
};
#define OP_PROP_TAG_ADVANCED ((eOperatorPropTags)OP_PROP_TAG_ADVANCED)

/* -------------------------------------------------------------------- */
/** \name #wmKeyMapItem
 * \{ */

/**
 * Modifier keys, not actually used for #wmKeyMapItem (never stored in DNA), used for:
 * - #wmEvent.modifier.
 * - #WM_keymap_add_item & #WM_modalkeymap_add_item
 */
enum wmEventModifierFlag : uint8_t {
  KM_SHIFT = (1 << 0),
  KM_CTRL = (1 << 1),
  KM_ALT = (1 << 2),
  /** Use for Windows-Key on MS-Windows, Command-key on macOS and Super on Linux. */
  KM_OSKEY = (1 << 3),
  /**
   * An additional modifier available on Unix systems (in addition to "Super").
   * Even though standard keyboards don't have a "Hyper" key it is a valid modifier
   * on Wayland and X11, where it is possible to map a key (typically CapsLock)
   * to be a Hyper modifier, see !136340.
   *
   * Note that this is currently only supported on Wayland & X11
   * but could be supported on other platforms if desired.
   */
  KM_HYPER = (1 << 4),
};
ENUM_OPERATORS(wmEventModifierFlag);

/** The number of modifiers #wmKeyMapItem & #wmEvent can use. */
#define KM_MOD_NUM 5

/**
 * #wmKeyMapItem.type
 * NOTE: most types are defined in `wm_event_types.hh`.
 */
enum {
  KM_TEXTINPUT = -2,
};

/** #wmKeyMapItem.val */
enum {
  KM_ANY = -1,
  KM_NOTHING = 0,
  KM_PRESS = 1,
  KM_RELEASE = 2,
  KM_CLICK = 3,
  KM_DBL_CLICK = 4,
  /**
   * \note The cursor location at the point dragging starts is set to #wmEvent.prev_press_xy
   * some operators such as box selection should use this location instead of #wmEvent.xy.
   */
  KM_PRESS_DRAG = 5,
};
/**
 * Alternate define for #wmKeyMapItem::shift and other modifiers.
 * While this matches the value of #KM_PRESS, modifiers should only be compared with:
 * (#KM_ANY, #KM_NOTHING, #KM_MOD_HELD).
 */
#define KM_MOD_HELD 1

/**
 * #wmKeyMapItem.direction
 *
 * Direction set for #KM_PRESS_DRAG key-map items. #KM_ANY (-1) to ignore direction.
 */
enum {
  KM_DIRECTION_N = 1,
  KM_DIRECTION_NE = 2,
  KM_DIRECTION_E = 3,
  KM_DIRECTION_SE = 4,
  KM_DIRECTION_S = 5,
  KM_DIRECTION_SW = 6,
  KM_DIRECTION_W = 7,
  KM_DIRECTION_NW = 8,
};

/** \} */

/* ************** UI Handler ***************** */

#define WM_UI_HANDLER_CONTINUE 0
#define WM_UI_HANDLER_BREAK 1

/* ************** Notifiers ****************** */

struct wmNotifier {
  wmNotifier *next, *prev;

  const wmWindow *window;

  unsigned int category, data, subtype, action;

  void *reference;
};

/* 4 levels
 *
 * 0xFF000000; category
 * 0x00FF0000; data
 * 0x0000FF00; data subtype (unused?)
 * 0x000000FF; action
 */

/* Category. */
#define NOTE_CATEGORY 0xFF000000
#define NOTE_CATEGORY_TAG_CLEARED NOTE_CATEGORY
#define NC_WM (1 << 24)
#define NC_WINDOW (2 << 24)
#define NC_WORKSPACE (3 << 24)
#define NC_SCREEN (4 << 24)
#define NC_SCENE (5 << 24)
#define NC_OBJECT (6 << 24)
#define NC_MATERIAL (7 << 24)
#define NC_TEXTURE (8 << 24)
#define NC_LAMP (9 << 24)
#define NC_GROUP (10 << 24)
#define NC_IMAGE (11 << 24)
#define NC_BRUSH (12 << 24)
#define NC_TEXT (13 << 24)
#define NC_WORLD (14 << 24)
#define NC_ANIMATION (15 << 24)
/* When passing a space as reference data with this (e.g. `WM_event_add_notifier(..., space)`),
 * the notifier will only be sent to this space. That avoids unnecessary updates for unrelated
 * spaces. */
#define NC_SPACE (16 << 24)
#define NC_GEOM (17 << 24)
#define NC_NODE (18 << 24)
#define NC_ID (19 << 24)
#define NC_PAINTCURVE (20 << 24)
#define NC_MOVIECLIP (21 << 24)
#define NC_MASK (22 << 24)
#define NC_GPENCIL (23 << 24)
#define NC_LINESTYLE (24 << 24)
#define NC_CAMERA (25 << 24)
#define NC_LIGHTPROBE (26 << 24)
/* Changes to asset data in the current .blend. */
#define NC_ASSET (27 << 24)
/* Changes to the active viewer path. */
#define NC_VIEWER_PATH (28 << 24)

/* Data type, 256 entries is enough, it can overlap. */
#define NOTE_DATA 0x00FF0000

/* NC_WM (window-manager). */
#define ND_FILEREAD (1 << 16)
#define ND_FILESAVE (2 << 16)
#define ND_DATACHANGED (3 << 16)
#define ND_HISTORY (4 << 16)
#define ND_JOB (5 << 16)
#define ND_UNDO (6 << 16)
#define ND_XR_DATA_CHANGED (7 << 16)
#define ND_LIB_OVERRIDE_CHANGED (8 << 16)

/* NC_SCREEN. */
#define ND_LAYOUTBROWSE (1 << 16)
#define ND_LAYOUTDELETE (2 << 16)
#define ND_ANIMPLAY (4 << 16)
#define ND_GPENCIL (5 << 16)
#define ND_LAYOUTSET (6 << 16)
#define ND_SKETCH (7 << 16)
#define ND_WORKSPACE_SET (8 << 16)
#define ND_WORKSPACE_DELETE (9 << 16)

/* NC_SCENE Scene. */
#define ND_SCENEBROWSE (1 << 16)
#define ND_MARKERS (2 << 16)
#define ND_FRAME (3 << 16)
#define ND_RENDER_OPTIONS (4 << 16)
#define ND_NODES (5 << 16)
#define ND_SEQUENCER (6 << 16)
/* NOTE: If an object was added, removed, merged/joined, ..., it is not enough to notify with
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
#define ND_WORLD (92 << 16)
#define ND_LAYER_CONTENT (101 << 16)

/* NC_OBJECT Object. */
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
/** For camera & sequencer viewport update, also with #NC_SCENE. */
#define ND_DRAW_RENDER_VIEWPORT (31 << 16)
#define ND_SHADERFX (32 << 16)
/* For updating motion paths in 3dview. */
#define ND_DRAW_ANIMVIZ (33 << 16)
#define ND_BONE_COLLECTION (34 << 16)

/* NC_MATERIAL Material. */
#define ND_SHADING (30 << 16)
#define ND_SHADING_DRAW (31 << 16)
#define ND_SHADING_LINKS (32 << 16)
#define ND_SHADING_PREVIEW (33 << 16)

/* NC_LAMP Light. */
#define ND_LIGHTING (40 << 16)
#define ND_LIGHTING_DRAW (41 << 16)

/* NC_WORLD World. */
#define ND_WORLD_DRAW (45 << 16)

/* NC_TEXT Text. */
#define ND_CURSOR (50 << 16)
#define ND_DISPLAY (51 << 16)

/* NC_ANIMATION Animato. */
#define ND_KEYFRAME (70 << 16)
#define ND_KEYFRAME_PROP (71 << 16)
#define ND_ANIMCHAN (72 << 16)
#define ND_NLA (73 << 16)
#define ND_NLA_ACTCHANGE (74 << 16)
#define ND_FCURVES_ORDER (75 << 16)
#define ND_NLA_ORDER (76 << 16)
#define ND_KEYFRAME_AUTO (77 << 16)

/* NC_GPENCIL. */
#define ND_GPENCIL_EDITMODE (85 << 16)

/* NC_GEOM Geometry. */
/* Mesh, Curve, MetaBall, Armature, etc. */
#define ND_SELECT (90 << 16)
#define ND_DATA (91 << 16)
#define ND_VERTEX_GROUP (92 << 16)

/* NC_NODE Nodes. */

/* Influences which menus node assets are included in. */
#define ND_NODE_ASSET_DATA (1 << 16)
#define ND_NODE_GIZMO (2 << 16)

/* NC_SPACE. */
#define ND_SPACE_CONSOLE (1 << 16)     /* General redraw. */
#define ND_SPACE_INFO_REPORT (2 << 16) /* Update for reports, could specify type. */
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
/* Sent to a new editor type after it's replaced an old one. */
#define ND_SPACE_CHANGED (19 << 16)
#define ND_SPACE_CLIP (20 << 16)
#define ND_SPACE_FILE_PREVIEW (21 << 16)
#define ND_SPACE_SPREADSHEET (22 << 16)
/* Not a space itself, but a part of another space. */
#define ND_REGIONS_ASSET_SHELF (23 << 16)

/* NC_ASSET. */
/* Denotes that the AssetList is done reading some previews. NOT that the preview generation of
 * assets is done. */
#define ND_ASSET_LIST (1 << 16)
#define ND_ASSET_LIST_PREVIEW (2 << 16)
#define ND_ASSET_LIST_READING (3 << 16)
/**
 * Catalog data changed, requiring a redraw of catalog UIs. Note that this doesn't denote a
 * reloading of asset libraries & their catalogs should happen.
 * That only happens on explicit user action.
 */
#define ND_ASSET_CATALOGS (4 << 16)

/* Subtype, 256 entries too. */
#define NOTE_SUBTYPE 0x0000FF00

/* Subtype scene mode. */
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
#define NS_EDITMODE_CURVES (11 << 8)
#define NS_EDITMODE_GREASE_PENCIL (12 << 8)
#define NS_EDITMODE_POINTCLOUD (13 << 8)

/* Subtype 3d view editing. */
#define NS_VIEW3D_GPU (16 << 8)
#define NS_VIEW3D_SHADING (17 << 8)

/* Subtype layer editing. */
#define NS_LAYER_COLLECTION (24 << 8)

/* Action classification. */
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

namespace blender::wm::gesture {
constexpr float POLYLINE_CLICK_RADIUS = 15.0f;
}

/** #wmGesture::type */
#define WM_GESTURE_LINES 1
#define WM_GESTURE_RECT 2
#define WM_GESTURE_CROSS_RECT 3
#define WM_GESTURE_LASSO 4
#define WM_GESTURE_CIRCLE 5
#define WM_GESTURE_STRAIGHTLINE 6
#define WM_GESTURE_POLYLINE 7

/**
 * wmGesture is registered to #wmWindow.gesture, handled by operator callbacks.
 */
struct wmGesture {
  wmGesture *next, *prev;
  /** #wmEvent.type. */
  int event_type;
  /** #wmEvent.modifier. */
  uint8_t event_modifier;
  /** #wmEvent.keymodifier. */
  short event_keymodifier;
  /** Gesture type define. */
  int type;
  /** Bounds of region to draw gesture within. */
  rcti winrct;
  /** Optional, amount of points stored. */
  int points;
  /** Optional, maximum amount of points stored. */
  int points_alloc;
  int modal_state;
  /** Optional, draw the active side of the straight-line gesture. */
  bool draw_active_side;
  /** Latest mouse position relative to area. Currently only used by lasso drawing code. */
  blender::int2 mval;

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
  /** Use for gestures that can be moved, like box selection. */
  uint move : 1;
  /** For gestures that support snapping, stores if snapping is enabled using the modal keymap
   * toggle. */
  uint use_snap : 1;
  /** For gestures that support flip, stores if flip is enabled using the modal keymap
   * toggle. */
  uint use_flip : 1;
  /** For gestures that support smoothing, stores if smoothing is enabled using the modal keymap
   * toggle. */
  uint use_smooth : 1;

  /**
   * customdata
   * - for border is a #rcti.
   * - for circle is #rcti, (`xmin`, `ymin`) is center, `xmax` radius.
   * - for lasso is short array.
   * - for straight line is a #rcti: (`xmin`, `ymin`) is start, (`xmax`, `ymax`) is end.
   */
  void *customdata;

  /** Free pointer to use for operator allocations (if set, its freed on exit). */
  wmGenericUserData user_data;
};

/* ************** wmEvent ************************ */

enum eWM_EventFlag {
  /**
   * True if the operating system inverted the delta x/y values and resulting
   * `prev_xy` values, for natural scroll direction.
   * For absolute scroll direction, the delta must be negated again.
   */
  WM_EVENT_SCROLL_INVERT = (1 << 0),
  /**
   * Generated by auto-repeat, note that this must only ever be set for keyboard events
   * where `ISKEYBOARD(event->type) == true`.
   *
   * See #KMI_REPEAT_IGNORE for details on how key-map handling uses this.
   */
  WM_EVENT_IS_REPEAT = (1 << 1),
  /**
   * Generated for consecutive trackpad or NDOF-motion events,
   * the repeat chain is broken by key/button events,
   * or cursor motion exceeding #WM_EVENT_CURSOR_MOTION_THRESHOLD.
   *
   * Changing the type of trackpad or gesture event also breaks the chain.
   */
  WM_EVENT_IS_CONSECUTIVE = (1 << 2),
  /**
   * Mouse-move events may have this flag set to force creating a click-drag event
   * even when the threshold has not been met.
   */
  WM_EVENT_FORCE_DRAG_THRESHOLD = (1 << 3),
};
ENUM_OPERATORS(eWM_EventFlag);

struct wmTabletData {
  /** 0=EVT_TABLET_NONE, 1=EVT_TABLET_STYLUS, 2=EVT_TABLET_ERASER. */
  int active;
  /** Range 0.0 (not touching) to 1.0 (full pressure). */
  float pressure;
  /**
   * X axis range: -1.0 (left) to +1.0 (right).
   * Y axis range: -1.0 (away from user) to +1.0 (toward user).
   */
  blender::float2 tilt;
  /** Interpret mouse motion as absolute as typical for tablets. */
  char is_motion_absolute;
};

/**
 * Each event should have full modifier state.
 * event comes from event manager and from keymap.
 *
 *
 * Previous State (`prev_*`)
 * =========================
 *
 * Events hold information about the previous event.
 *
 * - Previous values are only set for events types that generate #KM_PRESS.
 *   See: #ISKEYBOARD_OR_BUTTON.
 *
 * - Previous x/y are exceptions: #wmEvent.prev
 *   these are set on mouse motion, see #MOUSEMOVE & trackpad events.
 *
 * - Modal key-map handling sets `prev_val` & `prev_type` to `val` & `type`,
 *   this allows modal keys-maps to check the original values (needed in some cases).
 *
 *
 * Press State (`prev_press_*`)
 * ============================
 *
 * Events hold information about the state when the last #KM_PRESS event was added.
 * This is used for generating #KM_CLICK, #KM_DBL_CLICK & #KM_PRESS_DRAG events.
 * See #wm_handlers_do for the implementation.
 *
 * - Previous values are only set when a #KM_PRESS event is detected.
 *   See: #ISKEYBOARD_OR_BUTTON.
 *
 * - The reason to differentiate between "press" and the previous event state is
 *   the previous event may be set by key-release events. In the case of a single key click
 *   this isn't a problem however releasing other keys such as modifiers prevents click/click-drag
 *   events from being detected, see: #89989.
 *
 * - Mouse-wheel events are excluded even though they generate #KM_PRESS
 *   as clicking and dragging don't make sense for mouse wheel events.
 */
struct wmEvent {
  wmEvent *next, *prev;

  /** Event code itself (short, is also in key-map). */
  wmEventType type;
  /** Press, release, scroll-value. */
  short val;
  /** Mouse pointer position, screen coord. */
  int xy[2];
  /** Region relative mouse position (name convention before Blender 2.5). */
  int mval[2];
  /**
   * A single UTF8 encoded character.
   *
   * - Not null terminated although it may not be set `(utf8_buf[0] == '\0')`.
   * - #BLI_str_utf8_size_or_error() must _always_ return a valid value,
   *   check when assigning so we don't need to check on every access after.
   */
  char utf8_buf[6];

  /** Modifier states: #KM_SHIFT, #KM_CTRL, #KM_ALT, #KM_OSKEY & #KM_HYPER. */
  wmEventModifierFlag modifier;

  /** The direction (for #KM_PRESS_DRAG events only). */
  int8_t direction;

  /**
   * Raw-key modifier (allow using any key as a modifier).
   * Compatible with values in `type`.
   */
  wmEventType keymodifier;

  /** Tablet info, available for mouse move and button events. */
  wmTabletData tablet;

  eWM_EventFlag flag;

  /* Custom data. */

  /** Custom data type, stylus, 6-DOF, see `wm_event_types.hh`. */
  short custom;
  short customdata_free;
  /**
   * The #wmEvent::type implies the following #wmEvent::custodata.
   *
   * - #EVT_ACTIONZONE_AREA / #EVT_ACTIONZONE_FULLSCREEN / #EVT_ACTIONZONE_FULLSCREEN:
   *   Uses #sActionzoneData.
   * - #EVT_DROP: uses #ListBase of #wmDrag (also #wmEvent::custom == #EVT_DATA_DRAGDROP).
   *   Typically set to #wmWindowManger::drags.
   * - #EVT_FILESELECT: uses #wmOperator.
   * - #EVT_XR_ACTION: uses #wmXrActionData (also #wmEvent::custom == #EVT_DATA_XR).
   * - #NDOF_MOTION: uses #wmNDOFMotionData (also #wmEvent::custom == #EVT_DATA_NDOF_MOTION).
   * - #TIMER: uses #wmTimer (also #wmEvent::custom == #EVT_DATA_TIMER).
   */
  void *customdata;

  /* Previous State. */

  /** The previous value of `type`. */
  wmEventType prev_type;
  /** The previous value of `val`. */
  short prev_val;
  /**
   * The previous value of #wmEvent.xy,
   * Unlike other previous state variables, this is set on any mouse motion.
   * Use `prev_press_*` for the value at time of pressing.
   */
  int prev_xy[2];

  /* Previous Press State (when `val == KM_PRESS`). */

  /** The `type` at the point of the press action. */
  wmEventType prev_press_type;
  /**
   * The location when the key is pressed.
   * used to enforce drag threshold & calculate the `direction`.
   */
  int prev_press_xy[2];
  /** The `modifier` at the point of the press action. */
  wmEventModifierFlag prev_press_modifier;
  /** The `keymodifier` at the point of the press action. */
  wmEventType prev_press_keymodifier;
};

/**
 * Values below are ignored when detecting if the user intentionally moved the cursor.
 * Keep this very small since it's used for selection cycling for eg,
 * where we want intended adjustments to pass this threshold and select new items.
 *
 * Always check for <= this value since it may be zero.
 */
#define WM_EVENT_CURSOR_MOTION_THRESHOLD ((float)U.move_threshold * UI_SCALE_FAC)

/**
 * Motion progress, for modal handlers,
 * a copy of #GHOST_TProgress (keep in sync).
 */
enum wmProgress {
  P_NOT_STARTED = 0,
  /** Only sent for NDOF motion. */
  P_STARTING,
  /** Only sent for NDOF motion. */
  P_IN_PROGRESS,
  /** Only sent for NDOF motion. */
  P_FINISHING,
  P_FINISHED,
};

#ifdef WITH_INPUT_NDOF
/**
 * NDOF (3D mouse) motion event data.
 *
 * Awfully similar to #GHOST_TEventNDOFMotionData.
 */
struct wmNDOFMotionData {
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
  /**
   * Time since previous NDOF Motion event (in seconds).
   *
   * This is reset when motion begins: when progress changes from #P_NOT_STARTED to #P_STARTING.
   * In this case a dummy value is used, see #GHOST_NDOF_TIME_DELTA_STARTING.
   */
  float time_delta;
  /** Is this the first event, the last, or one of many in between? */
  wmProgress progress;
};
#endif /* WITH_INPUT_NDOF */

#ifdef WITH_XR_OPENXR
/** Similar to #GHOST_XrPose. */
struct wmXrPose {
  float position[3];
  /** Blender convention (w, x, y, z). */
  float orientation_quat[4];
};

struct wmXrActionState {
  union {
    bool state_boolean;
    float state_float;
    float state_vector2f[2];
    wmXrPose state_pose;
  };
  int type; /* #eXrActionType. */
};

struct wmXrActionData {
  /** Action set name. */
  char action_set[64];
  /** Action name. */
  char action[64];
  /** User path. E.g. "/user/hand/left". */
  char user_path[64];
  /** Other user path, for bimanual actions. E.g. "/user/hand/right". */
  char user_path_other[64];
  /** Type. */
  eXrActionType type;
  /** State. Set appropriately based on type. */
  float state[2];
  /** State of the other sub-action path for bimanual actions. */
  float state_other[2];

  /** Input threshold for float/vector2f actions. */
  float float_threshold;

  /** Controller aim pose corresponding to the action's sub-action path. */
  float controller_loc[3];
  float controller_rot[4];
  /** Controller aim pose of the other sub-action path for bimanual actions. */
  float controller_loc_other[3];
  float controller_rot_other[4];

  /** Operator. */
  wmOperatorType *ot;
  IDProperty *op_properties;

  /** Whether bimanual interaction is occurring. */
  bool bimanual;
};
#endif

/** Timer flags. */
enum wmTimerFlags {
  /** Do not attempt to free custom-data pointer even if non-NULL. */
  WM_TIMER_NO_FREE_CUSTOM_DATA = 1 << 0,

  /* Internal flags, should not be used outside of WM code. */
  /**
   * This timer has been tagged for removal and deletion, handled by WM code to ensure timers are
   * deleted in a safe context.
   */
  WM_TIMER_TAGGED_FOR_REMOVAL = 1 << 16,
};
ENUM_OPERATORS(wmTimerFlags)

struct wmTimer {
  wmTimer *next, *prev;

  /** Window this timer is attached to (optional). */
  wmWindow *win;

  /** Set by timer user. */
  double time_step;
  /** Set by timer user, goes to event system. */
  wmEventType event_type;
  /** Various flags controlling timer options, see below. */
  wmTimerFlags flags;
  /** Set by timer user, to allow custom values. */
  void *customdata;

  /** Total running time in seconds. */
  double time_duration;
  /** Time since previous step in seconds. */
  double time_delta;

  /** Internal, last time timer was activated. */
  double time_last;
  /** Internal, next time we want to activate the timer. */
  double time_next;
  /** Internal, when the timer started. */
  double time_start;
  /** Internal, put timers to sleep when needed. */
  bool sleep;
};

enum wmPopupSize {
  WM_POPUP_SIZE_SMALL = 0,
  WM_POPUP_SIZE_LARGE,
};

enum wmPopupPosition {
  WM_POPUP_POSITION_MOUSE = 0,
  WM_POPUP_POSITION_CENTER,
};

/**
 * Communication/status data owned by the wmJob, and passed to the worker code when calling
 * `startjob` callback.
 *
 * `OUTPUT` members mean that they are defined by the worker thread, and read/used by the wmJob
 * management code from the main thread. And vice-versa for `INPUT` members.
 *
 * \warning There is currently no thread-safety or synchronization when accessing these values.
 * This is fine as long as:
 *   - All members are independent of each other, value-wise.
 *   - Each member is 'simple enough' that accessing it or setting it can be considered as atomic.
 *   - There is no requirement of immediate synchronization of these values between the main
 *     controlling thread (i.e. wmJob management code) and the worker thread.
 */
struct wmJobWorkerStatus {
  /**
   * OUTPUT - Set to true by the worker to request update processing from the main thread (as part
   * of the wmJob 'event loop', see #wm_jobs_timer).
   */
  bool do_update;

  /**
   * INPUT - Set by the wmJob management code to request a worker to stop/abort its processing.
   *
   * \note Some job types (rendering or baking ones e.g.) also use the #Global.is_break flag to
   * cancel their processing.
   */
  bool stop;

  /** OUTPUT - Progress as reported by the worker, from `0.0f` to `1.0f`. */
  float progress;

  /**
   * OUTPUT - Storage of reports generated during this job's run. Contains its own locking for
   * thread-safety.
   */
  ReportList *reports;
};

struct wmOperatorType {
  /** Text for UI, undo (should not exceed #OP_MAX_TYPENAME). */
  const char *name = nullptr;
  /** Unique identifier (must not exceed #OP_MAX_TYPENAME). */
  const char *idname = nullptr;
  /** Translation context (must not exceed #BKE_ST_MAXNAME). */
  const char *translation_context = nullptr;
  /** Use for tooltips and Python docs. */
  const char *description = nullptr;
  /** Identifier to group operators together. */
  const char *undo_group = nullptr;

  /**
   * This callback executes the operator without any interactive input,
   * parameters may be provided through operator properties. cannot use
   * any interface code or input device state.
   * See defines below for return values.
   */
  wmOperatorStatus (*exec)(bContext *C, wmOperator *op) ATTR_WARN_UNUSED_RESULT = nullptr;

  /**
   * This callback executes on a running operator whenever as property
   * is changed. It can correct its own properties or report errors for
   * invalid settings in exceptional cases.
   * Boolean return value, True denotes a change has been made and to redraw.
   */
  bool (*check)(bContext *C, wmOperator *op) = nullptr;

  /**
   * For modal temporary operators, initially invoke is called, then
   * any further events are handled in #modal. If the operation is
   * canceled due to some external reason, cancel is called
   * See defines below for return values.
   */
  wmOperatorStatus (*invoke)(bContext *C,
                             wmOperator *op,
                             const wmEvent *event) ATTR_WARN_UNUSED_RESULT = nullptr;

  /**
   * Called when a modal operator is canceled (not used often).
   * Internal cleanup can be done here if needed.
   */
  void (*cancel)(bContext *C, wmOperator *op) = nullptr;

  /**
   * Modal is used for operators which continuously run. Fly mode, knife tool, circle select are
   * all examples of modal operators. Modal operators can handle events which would normally invoke
   * or execute other operators. They keep running until they don't return
   * `OPERATOR_RUNNING_MODAL`.
   */
  wmOperatorStatus (*modal)(bContext *C,
                            wmOperator *op,
                            const wmEvent *event) ATTR_WARN_UNUSED_RESULT = nullptr;

  /**
   * Verify if the operator can be executed in the current context. Note
   * that the operator may still fail to execute even if this returns true.
   */
  bool (*poll)(bContext *C) ATTR_WARN_UNUSED_RESULT = nullptr;

  /**
   * Used to check if properties should be displayed in auto-generated UI.
   * Use 'check' callback to enforce refreshing.
   */
  bool (*poll_property)(const bContext *C,
                        wmOperator *op,
                        const PropertyRNA *prop) ATTR_WARN_UNUSED_RESULT = nullptr;

  /** Optional panel for redo and repeat, auto-generated if not set. */
  void (*ui)(bContext *C, wmOperator *op) = nullptr;
  /**
   * Optional check for whether the #ui callback should be called (usually to create the redo
   * panel interface).
   */
  bool (*ui_poll)(wmOperatorType *ot, PointerRNA *ptr) = nullptr;

  /**
   * Return a different name to use in the user interface, based on property values.
   * The returned string is expected to be translated if needed.
   *
   * WARNING: This callback does not currently work as expected in most common usage cases (e.g.
   * any definition of an operator button through the layout API will fail to execute it). See
   * #112253 for details.
   */
  std::string (*get_name)(wmOperatorType *ot, PointerRNA *ptr) = nullptr;

  /**
   * Return a different description to use in the user interface, based on property values.
   * The returned string is expected to be translated if needed.
   */
  std::string (*get_description)(bContext *C, wmOperatorType *ot, PointerRNA *ptr) = nullptr;

  /** A dynamic version of #OPTYPE_DEPENDS_ON_CURSOR which can depend on operator properties. */
  bool (*depends_on_cursor)(bContext &C, wmOperatorType &ot, PointerRNA *ptr) = nullptr;

  /** RNA for properties. */
  StructRNA *srna = nullptr;

  /** Previous settings - for initializing on re-use. */
  IDProperty *last_properties = nullptr;

  /**
   * Default rna property to use for generic invoke functions.
   * menus, enum search... etc. Example: Enum 'type' for a Delete menu.
   *
   * When assigned a string/number property,
   * immediately edit the value when used in a popup. see: #UI_BUT_ACTIVATE_ON_INIT.
   */
  PropertyRNA *prop = nullptr;

  /** #wmOperatorTypeMacro. */
  ListBase macro = {};

  /** Pointer to modal keymap. Do not free! */
  wmKeyMap *modalkeymap = nullptr;

  /** Python needs the operator type as well. */
  bool (*pyop_poll)(bContext *C, wmOperatorType *ot) ATTR_WARN_UNUSED_RESULT = nullptr;

  /** RNA integration. */
  ExtensionRNA rna_ext = {};

  /** Cursor to use when waiting for cursor input, see: #OPTYPE_DEPENDS_ON_CURSOR. */
  int cursor_pending = 0;

  /** Flag last for padding. */
  short flag = 0;
};

/**
 * Wrapper to reference a #wmOperatorType together with some set properties and other relevant
 * information to invoke the operator in a customizable way.
 */
struct wmOperatorCallParams {
  wmOperatorType *optype;
  PointerRNA *opptr;
  blender::wm::OpCallContext opcontext;
};

#ifdef WITH_INPUT_IME
/* *********** Input Method Editor (IME) *********** */
/**
 * \warning this is a duplicate of #GHOST_TEventImeData.
 * All members must remain aligned and the struct size match!
 */
struct wmIMEData {
  /** UTF8 encoding. */
  std::string result;
  /** UTF8 encoding. */
  std::string composite;

  /** Cursor position in the IME composition. */
  int cursor_pos;
  /** Beginning of the selection. */
  int sel_start;
  /** End of the selection. */
  int sel_end;
};
#endif

/* **************** Paint Cursor ******************* */

using wmPaintCursorDraw = void (*)(bContext *C,
                                   const blender::int2 &xy,
                                   const blender::float2 &tilt,
                                   void *customdata);

/* *************** Drag and drop *************** */

enum eWM_DragDataType : int8_t {
  WM_DRAG_ID,
  WM_DRAG_ASSET,
  /** The user is dragging multiple assets. This is only supported in few specific cases, proper
   * multi-item support for dragging isn't supported well yet. Therefore this is kept separate from
   * #WM_DRAG_ASSET. */
  WM_DRAG_ASSET_LIST,
  WM_DRAG_RNA,
  WM_DRAG_PATH,
  WM_DRAG_NAME,
  /**
   * Arbitrary text such as dragging from a text editor,
   * this is also used when dragging a URL from a browser.
   *
   * An #std::string expected to be UTF8 encoded.
   * Callers that require valid UTF8 sequences must validate the text.
   */
  WM_DRAG_STRING,
  WM_DRAG_COLOR,
  WM_DRAG_DATASTACK,
  WM_DRAG_ASSET_CATALOG,
  WM_DRAG_GREASE_PENCIL_LAYER,
  WM_DRAG_GREASE_PENCIL_GROUP,
  WM_DRAG_NODE_TREE_INTERFACE,
  WM_DRAG_BONE_COLLECTION,
  WM_DRAG_SHAPE_KEY,
};

enum eWM_DragFlags {
  WM_DRAG_NOP = 0,
  WM_DRAG_FREE_DATA = 1,
};
ENUM_OPERATORS(eWM_DragFlags)

/* NOTE: structs need not exported? */

struct wmDragID {
  wmDragID *next, *prev;
  ID *id;
  ID *from_parent;
};

struct wmDragAsset {
  const AssetRepresentationHandle *asset;
  AssetImportSettings import_settings;
};

struct wmDragAssetCatalog {
  bUUID drag_catalog_id;
};

/**
 * For some specific cases we support dragging multiple assets (#WM_DRAG_ASSET_LIST). There is no
 * proper support for dragging multiple items in the `wmDrag`/`wmDrop` API yet, so this is really
 * just to enable specific features for assets.
 *
 * This struct basically contains a tagged union to either store a local ID pointer, or information
 * about an externally stored asset.
 */
struct wmDragAssetListItem {
  wmDragAssetListItem *next, *prev;

  union {
    ID *local_id;
    wmDragAsset *external_info;
  } asset_data;

  bool is_external;
};

struct wmDragPath {
  blender::Vector<std::string> paths;
  /** File type of each path in #paths. */
  blender::Vector<int> file_types; /* #eFileSel_File_Types. */
  /** Bit flag of file types in #paths. */
  int file_types_bit_flag; /* #eFileSel_File_Types. */
  std::string tooltip;
};

struct wmDragGreasePencilLayer {
  GreasePencil *grease_pencil;
  GreasePencilLayerTreeNode *node;
};

using WMDropboxTooltipFunc = std::string (*)(bContext *C,
                                             wmDrag *drag,
                                             const int xy[2],
                                             wmDropBox *drop);

struct wmDragActiveDropState {
  wmDragActiveDropState();
  ~wmDragActiveDropState();

  /**
   * Informs which dropbox is activated with the drag item.
   * When this value changes, the #on_enter() and #on_exit() dropbox callbacks are triggered.
   */
  wmDropBox *active_dropbox;

  /**
   * If `active_dropbox` is set, the area it successfully polled in.
   * To restore the context of it as needed.
   */
  ScrArea *area_from;
  /**
   * If `active_dropbox` is set, the region it successfully polled in.
   * To restore the context of it as needed.
   */
  ARegion *region_from;

  /**
   * If `active_dropbox` is set, additional context provided by the active (i.e. hovered) button.
   * Activated before context sensitive operations (polling, drawing, dropping).
   */
  std::unique_ptr<bContextStore> ui_context;

  /**
   * Text to show when a dropbox poll succeeds (so the dropbox itself is available) but the
   * operator poll fails. Typically the message the operator set with
   * #CTX_wm_operator_poll_msg_set().
   */
  const char *disabled_info;
  bool free_disabled_info;

  std::string tooltip;
};

struct wmDrag {
  wmDrag *next, *prev;

  int icon;
  eWM_DragDataType type;
  void *poin;

  /** If no small icon but imbuf should be drawn around cursor. */
  const ImBuf *imb;
  float imbuf_scale;
  /** If #imb is not set, draw this as a big preview instead of the small #icon. */
  int preview_icon_id; /* BIFIconID */

  wmDragActiveDropState drop_state;

  eWM_DragFlags flags;

  /** List of wmDragIDs, all are guaranteed to have the same ID type. */
  ListBase ids;
  /** List of `wmDragAssetListItem`s. */
  ListBase asset_items;
};

/**
 * Drop-boxes are like key-maps, part of the screen/area/region definition.
 * Allocation and free is on startup and exit.
 *
 * The operator is polled and invoked with the current context
 * (#blender::wm::OpCallContext::InvokeDefault), there is no way to override that (by design, since
 * drop-boxes should act on the exact mouse position). So the drop-boxes are supposed to check the
 * required area and region context in their poll.
 */
struct wmDropBox {
  wmDropBox *next, *prev;

  /** Test if the dropbox is active. */
  bool (*poll)(bContext *C, wmDrag *drag, const wmEvent *event);

  /** Called when the drag action starts. Can be used to prefetch data for previews.
   * \note The dropbox that will be called eventually is not known yet when starting the drag.
   * So this callback is called on every dropbox that is registered in the current screen. */
  void (*on_drag_start)(bContext *C, wmDrag *drag);

  /** Called when poll returns true the first time. Typically used to setup some drawing data. */
  void (*on_enter)(wmDropBox *drop, wmDrag *drag);

  /** Called when poll returns false the first time or when the drag event ends (successful drop or
   * canceled). Typically used to cleanup resources or end drawing. */
  void (*on_exit)(wmDropBox *drop, wmDrag *drag);

  /** Before exec, this copies drag info to #wmDrop properties. */
  void (*copy)(bContext *C, wmDrag *drag, wmDropBox *drop);

  /**
   * If the operator is canceled (returns `OPERATOR_CANCELLED`), this can be used for cleanup of
   * `copy()` resources.
   */
  void (*cancel)(Main *bmain, wmDrag *drag, wmDropBox *drop);

  /**
   * Override the default cursor overlay drawing function.
   * Can be used to draw text or thumbnails. IE a tool-tip for drag and drop.
   * \param xy: Cursor location in window coordinates (#wmEvent.xy compatible).
   */
  void (*draw_droptip)(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2]);

  /**
   * Called with the draw buffer (#GPUViewport) set up for drawing into the region's view.
   * \note Only setups the drawing buffer for drawing in view, not the GPU transform matrices.
   * The callback has to do that itself, with for example #UI_view2d_view_ortho.
   * \param xy: Cursor location in window coordinates (#wmEvent.xy compatible).
   */
  void (*draw_in_view)(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2]);

  /** Custom data for drawing. */
  void *draw_data;

  /** Custom tool-tip shown during dragging. */
  WMDropboxTooltipFunc tooltip;

  /**
   * If poll succeeds, operator is called.
   * Not saved in file, so can be pointer.
   * This may be null when the operator has been unregistered,
   * where `opname` can be used to re-initialize it.
   */
  wmOperatorType *ot;
  /** #wmOperatorType::idname, needed for re-registration. */
  char opname[64];

  /** Operator properties, assigned to `ptr->data` and can be written to a file. */
  IDProperty *properties;
  /** RNA pointer to access properties. */
  PointerRNA *ptr;
};

/**
 * Struct to store tool-tip timer and possible creation if the time is reached.
 * Allows UI code to call #WM_tooltip_timer_init without each user having to handle the timer.
 */
struct wmTooltipState {
  /** Create tool-tip on this event. */
  wmTimer *timer;
  /** The area the tool-tip is created in. */
  ScrArea *area_from;
  /** The region the tool-tip is created in. */
  ARegion *region_from;
  /** The tool-tip region. */
  ARegion *region;
  /** Create the tool-tip region (assign to 'region'). */
  ARegion *(*init)(
      bContext *C, ARegion *region, int *pass, double *pass_delay, bool *r_exit_on_event);
  /** Exit on any event, not needed for buttons since their highlight state is used. */
  bool exit_on_event;
  /** Cursor location at the point of tool-tip creation. */
  int event_xy[2];
  /** Pass, use when we want multiple tips, count down to zero. */
  int pass;
};

/* *************** migrated stuff, clean later? ************** */

struct RecentFile {
  RecentFile *next, *prev;
  char *filepath;
};

/* Logging. */
struct CLG_LogRef;
/* `wm_init_exit.cc`. */

extern CLG_LogRef *WM_LOG_OPERATORS;
extern CLG_LogRef *WM_LOG_EVENTS;
extern CLG_LogRef *WM_LOG_TOOL_GIZMO;
extern CLG_LogRef *WM_LOG_MSGBUS_PUB;
extern CLG_LogRef *WM_LOG_MSGBUS_SUB;
