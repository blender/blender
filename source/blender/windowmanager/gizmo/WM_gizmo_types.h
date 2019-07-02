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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * \name Gizmo Types
 * \brief Gizmo defines for external use.
 *
 * Only included in WM_types.h and lower level files.
 */

#ifndef __WM_GIZMO_TYPES_H__
#define __WM_GIZMO_TYPES_H__

#include "BLI_compiler_attrs.h"

struct wmGizmo;
struct wmGizmoGroup;
struct wmGizmoGroupType;
struct wmGizmoMapType;
struct wmGizmoProperty;
struct wmKeyConfig;

#include "DNA_listBase.h"

/* -------------------------------------------------------------------- */
/* Enum Typedef's */

/**
 * #wmGizmo.state
 */
typedef enum eWM_GizmoFlagState {
  /** While hovered. */
  WM_GIZMO_STATE_HIGHLIGHT = (1 << 0),
  /** While dragging. */
  WM_GIZMO_STATE_MODAL = (1 << 1),
  WM_GIZMO_STATE_SELECT = (1 << 2),
} eWM_GizmoFlagState;

/**
 * #wmGizmo.flag
 * Flags for individual gizmos.
 */
typedef enum eWM_GizmoFlag {
  /** Draw *only* while hovering. */
  WM_GIZMO_DRAW_HOVER = (1 << 0),
  /** Draw while dragging. */
  WM_GIZMO_DRAW_MODAL = (1 << 1),
  /** Draw an indicator for the current value while dragging. */
  WM_GIZMO_DRAW_VALUE = (1 << 2),
  WM_GIZMO_HIDDEN = (1 << 3),
  WM_GIZMO_HIDDEN_SELECT = (1 << 4),
  /**
   * When set 'scale_final' value also scales the offset.
   * Use when offset is to avoid screen-space overlap instead of absolute positioning. */
  WM_GIZMO_DRAW_OFFSET_SCALE = (1 << 5),
  /**
   * User should still use 'scale_final' for any handles and UI elements.
   * This simply skips scale when calculating the final matrix.
   * Needed when the gizmo needs to align with the interface underneath it. */
  WM_GIZMO_DRAW_NO_SCALE = (1 << 6),
  /**
   * Hide the cursor and lock it's position while interacting with this gizmo.
   */
  WM_GIZMO_MOVE_CURSOR = (1 << 7),
  /** Don't write into the depth buffer when selecting. */
  WM_GIZMO_SELECT_BACKGROUND = (1 << 8),

  /** Use the active tools operator properties when running as an operator. */
  WM_GIZMO_OPERATOR_TOOL_INIT = (1 << 9),

  /** Don't pass through events to other handlers
   * (allows click/drag not to have it's events stolen by press events in other keymaps). */
  WM_GIZMO_EVENT_HANDLE_ALL = (1 << 10),
} eWM_GizmoFlag;

/**
 * #wmGizmoGroupType.flag
 * Flags that influence the behavior of all gizmos in the group.
 */
typedef enum eWM_GizmoFlagGroupTypeFlag {
  /** Mark gizmo-group as being 3D */
  WM_GIZMOGROUPTYPE_3D = (1 << 0),
  /** Scale gizmos as 3D object that respects zoom (otherwise zoom independent draw size).
   * note: currently only for 3D views, 2D support needs adding. */
  WM_GIZMOGROUPTYPE_SCALE = (1 << 1),
  /** Gizmos can be depth culled with scene objects (covered by other geometry - TODO) */
  WM_GIZMOGROUPTYPE_DEPTH_3D = (1 << 2),
  /** Gizmos can be selected. */
  WM_GIZMOGROUPTYPE_SELECT = (1 << 3),
  /** The gizmo group is to be kept (not removed on loading a new file for eg). */
  WM_GIZMOGROUPTYPE_PERSISTENT = (1 << 4),
  /** Show all other gizmos when interacting. */
  WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL = (1 << 5),
  /**
   * When used with tool, only run when activating the tool,
   * instead of linking the gizmo while the tool is active.
   *
   * \warning this option has some limitations, we might even re-implement this differently.
   * Currently it's quite minimal so we can see how it works out.
   * The main issue is controlling how a gizmo is activated with a tool
   * when a tool can activate multiple operators based on the key-map.
   * We could even move the options into the key-map item.
   * ~ campbell. */
  WM_GIZMOGROUPTYPE_TOOL_INIT = (1 << 6),
} eWM_GizmoFlagGroupTypeFlag;

/**
 * #wmGizmoGroup.init_flag
 */
typedef enum eWM_GizmoFlagGroupInitFlag {
  /** Gizmo-group has been initialized. */
  WM_GIZMOGROUP_INIT_SETUP = (1 << 0),
  WM_GIZMOGROUP_INIT_REFRESH = (1 << 1),
} eWM_GizmoFlagGroupInitFlag;

/**
 * #wmGizmoMapType.type_update_flag
 * Gizmo-map type update flag
 */
typedef enum eWM_GizmoFlagMapTypeUpdateFlag {
  /** A new type has been added, needs to be initialized for all views. */
  WM_GIZMOMAPTYPE_UPDATE_INIT = (1 << 0),
  WM_GIZMOMAPTYPE_UPDATE_REMOVE = (1 << 1),

  /** Needed because keymap may be registered before and after window initialization.
   * So we need to keep track of keymap initialization separately. */
  WM_GIZMOMAPTYPE_KEYMAP_INIT = (1 << 2),
} eWM_GizmoFlagMapTypeUpdateFlag;

/* -------------------------------------------------------------------- */
/* wmGizmo */

/**
 * \brief Gizmo tweak flag.
 * Bitflag passed to gizmo while tweaking.
 *
 * \note Gizmos are responsible for handling this #wmGizmo.modal callback!.
 */
typedef enum {
  /* Drag with extra precision (Shift). */
  WM_GIZMO_TWEAK_PRECISE = (1 << 0),
  /* Drag with snap enabled (Ctrl).  */
  WM_GIZMO_TWEAK_SNAP = (1 << 1),
} eWM_GizmoFlagTweak;

#include "wm_gizmo_fn.h"

typedef struct wmGizmoOpElem {
  struct wmOperatorType *type;
  /** Operator properties if gizmo spawns and controls an operator,
   * or owner pointer if gizmo spawns and controls a property. */
  PointerRNA ptr;

  bool is_redo;
} wmGizmoOpElem;

/* gizmos are set per region by registering them on gizmo-maps */
struct wmGizmo {
  struct wmGizmo *next, *prev;

  /** While we don't have a real type, use this to put type-like vars. */
  const struct wmGizmoType *type;

  /** Overrides 'type->modal' when set.
   * Note that this is a workaround, remove if we can. */
  wmGizmoFnModal custom_modal;

  /** Pointer back to group this gizmo is in (just for quick access). */
  struct wmGizmoGroup *parent_gzgroup;

  /** Optional keymap to use for this gizmo (overrides #wmGizmoGroupType.keymap) */
  struct wmKeyMap *keymap;

  void *py_instance;

  /** Rna pointer to access properties. */
  struct PointerRNA *ptr;

  /** Flags that influence the behavior or how the gizmos are drawn. */
  eWM_GizmoFlag flag;
  /** State flags (active, highlighted, selected) */
  eWM_GizmoFlagState state;

  /** Optional ID for highlighting different parts of this gizmo.
   * -1 when unset, otherwise a valid index. (Used as index to 'op_data'). */
  int highlight_part;

  /**
   * For gizmos that differentiate between click & drag,
   * use a different part for any drag events, -1 when unused.
   */
  int drag_part;

  /** Distance to bias this gizmo above others when picking
   * (in worldspace, scaled by the gizmo scale - when used). */
  float select_bias;

  /**
   * Transformation of the gizmo in 2d or 3d space.
   * - Matrix axis are expected to be unit length (scale is applied after).
   * - Behavior when axis aren't orthogonal depends on each gizmo.
   * - Typically the +Z is the primary axis for gizmos to use.
   * - 'matrix[3]' must be used for location,
   *   besides this it's up to the gizmos internal code how the
   *   rotation components are used for drawing and interaction.
   */

  /** The space this gizmo is being modified in. */
  float matrix_space[4][4];
  /** Transformation of this gizmo. */
  float matrix_basis[4][4];
  /** Custom offset from origin. */
  float matrix_offset[4][4];
  /** Runtime property, set the scale while drawing on the viewport. */
  float scale_final;
  /** User defined scale, in addition to the original one. */
  float scale_basis;
  /** User defined width for line drawing. */
  float line_width;
  /** Gizmo colors (uses default fallbacks if not defined). */
  float color[4], color_hi[4];

  /** Data used during interaction. */
  void *interaction_data;

  /** Operator to spawn when activating the gizmo (overrides property editing),
   * an array of items (aligned with #wmGizmo.highlight_part). */
  wmGizmoOpElem *op_data;
  int op_data_len;

  struct IDProperty *properties;

  /** Redraw tag. */
  bool do_draw;

  /** Temporary data (assume dirty). */
  union {
    float f;
  } temp;

  /* over alloc target_properties after 'wmGizmoType.struct_size' */
};

/** Similar to #PropertyElemRNA, but has an identifier. */
typedef struct wmGizmoProperty {
  const struct wmGizmoPropertyType *type;

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* Optional functions for converting to/from RNA  */
  struct {
    wmGizmoPropertyFnGet value_get_fn;
    wmGizmoPropertyFnSet value_set_fn;
    wmGizmoPropertyFnRangeGet range_get_fn;
    wmGizmoPropertyFnFree free_fn;
    void *user_data;
  } custom_func;
} wmGizmoProperty;

typedef struct wmGizmoPropertyType {
  struct wmGizmoPropertyType *next, *prev;
  /** #PropertyType, typically #PROP_FLOAT. */
  int data_type;
  int array_length;

  /* index within 'wmGizmoType' */
  int index_in_type;

  /** over alloc. */
  char idname[0];
} wmGizmoPropertyType;

/**
 * Simple utility wrapper for storing a single gizmo as wmGizmoGroup.customdata (which gets freed).
 */
typedef struct wmGizmoWrapper {
  struct wmGizmo *gizmo;
} wmGizmoWrapper;

struct wmGizmoMapType_Params {
  short spaceid;
  short regionid;
};

typedef struct wmGizmoType {

  const char *idname; /* MAX_NAME */

  /** Set to 'sizeof(wmGizmo)' or larger for instances of this type,
   * use so we can cant to other types without the hassle of a custom-data pointer. */
  uint struct_size;

  /** Initialize struct (calloc'd 'struct_size' region). */
  wmGizmoFnSetup setup;

  /** Draw gizmo. */
  wmGizmoFnDraw draw;

  /** Determines 3d intersection by rendering the gizmo in a selection routine. */
  wmGizmoFnDrawSelect draw_select;

  /** Determine if the mouse intersects with the gizmo.
   * The calculation should be done in the callback itself, -1 for no selection. */
  wmGizmoFnTestSelect test_select;

  /** Handler used by the gizmo. Usually handles interaction tied to a gizmo type. */
  wmGizmoFnModal modal;

  /** Gizmo-specific handler to update gizmo attributes based on the property value. */
  wmGizmoFnPropertyUpdate property_update;

  /** Returns the final transformation which may be different from the 'matrix',
   * depending on the gizmo.
   * Notes:
   * - Scale isn't applied (wmGizmo.scale/user_scale).
   * - Offset isn't applied (wmGizmo.matrix_offset).
   */
  wmGizmoFnMatrixBasisGet matrix_basis_get;

  /** Activate a gizmo state when the user clicks on it. */
  wmGizmoFnInvoke invoke;

  /** Called when gizmo tweaking is done - used to free data and reset property when cancelling. */
  wmGizmoFnExit exit;

  wmGizmoFnCursorGet cursor_get;

  /** Called when gizmo selection state changes. */
  wmGizmoFnSelectRefresh select_refresh;

  /** Free data (not the gizmo it's self), use when the gizmo allocates it's own members. */
  wmGizmoFnFree free;

  /** RNA for properties. */
  struct StructRNA *srna;

  /** RNA integration. */
  ExtensionRNA ext;

  ListBase target_property_defs;
  int target_property_defs_len;

} wmGizmoType;

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

/** Factory class for a gizmo-group type, gets called every time a new area is spawned. */
typedef struct wmGizmoGroupTypeRef {
  struct wmGizmoGroupTypeRef *next, *prev;
  struct wmGizmoGroupType *type;
} wmGizmoGroupTypeRef;

/* factory class for a gizmo-group type, gets called every time a new area is spawned */
typedef struct wmGizmoGroupType {
  const char *idname; /* MAX_NAME */
  /** Gizmo-group name - displayed in UI (keymap editor). */
  const char *name;
  char owner_id[64]; /* MAX_NAME */

  /** Poll if gizmo-map should be visible. */
  wmGizmoGroupFnPoll poll;
  /** Initially create gizmos and set permanent data - stuff you only need to do once. */
  wmGizmoGroupFnInit setup;
  /** Refresh data, only called if recreate flag is set (WM_gizmomap_tag_refresh). */
  wmGizmoGroupFnRefresh refresh;
  /** Refresh data for drawing, called before each redraw. */
  wmGizmoGroupFnDrawPrepare draw_prepare;
  /** Initialize data for before invoke. */
  wmGizmoGroupFnInvokePrepare invoke_prepare;

  /** Keymap init callback for this gizmo-group (optional),
   * will fall back to default tweak keymap when left NULL. */
  wmGizmoGroupFnSetupKeymap setup_keymap;

  /** Optionally subscribe to wmMsgBus events,
   * these are calculated automatically from RNA properties,
   * only needed if gizmos depend indirectly on properties. */
  wmGizmoGroupFnMsgBusSubscribe message_subscribe;

  /** Keymap created with callback from above. */
  struct wmKeyMap *keymap;
  /** Only for convenient removal. */
  struct wmKeyConfig *keyconf;

  /* Note: currently gizmo-group instances don't store properties,
   * they're kept in the tool properties. */

  /** RNA for properties. */
  struct StructRNA *srna;

  /** RNA integration. */
  ExtensionRNA ext;

  eWM_GizmoFlagGroupTypeFlag flag;

  /** So we know which group type to update. */
  eWM_GizmoFlagMapTypeUpdateFlag type_update_flag;

  /** Same as gizmo-maps, so registering/unregistering goes to the correct region. */
  struct wmGizmoMapType_Params gzmap_params;

  /**
   * Number of #wmGizmoGroup instances.
   * Decremented when 'tag_remove' is set, or when removed.
   */
  int users;

} wmGizmoGroupType;

typedef struct wmGizmoGroup {
  struct wmGizmoGroup *next, *prev;

  struct wmGizmoGroupType *type;
  ListBase gizmos;

  struct wmGizmoMap *parent_gzmap;

  /** Python stores the class instance here. */
  void *py_instance;
  /** Errors and warnings storage. */
  struct ReportList *reports;

  bool tag_remove;

  void *customdata;
  /** For freeing customdata from above. */
  void (*customdata_free)(void *);
  eWM_GizmoFlagGroupInitFlag init_flag;
} wmGizmoGroup;

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

/**
 * Pass a value of this enum to #WM_gizmomap_draw to tell it what to draw.
 */
typedef enum eWM_GizmoFlagMapDrawStep {
  /** Draw 2D gizmo-groups (#WM_GIZMOGROUPTYPE_3D not set). */
  WM_GIZMOMAP_DRAWSTEP_2D = 0,
  /** Draw 3D gizmo-groups (#WM_GIZMOGROUPTYPE_3D set). */
  WM_GIZMOMAP_DRAWSTEP_3D,
} eWM_GizmoFlagMapDrawStep;
#define WM_GIZMOMAP_DRAWSTEP_MAX 2

#endif /* __WM_GIZMO_TYPES_H__ */
