/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/WM_manipulator_types.h
 *  \ingroup wm
 *
 * \name Manipulator Types
 * \brief Manipulator defines for external use.
 *
 * Only included in WM_types.h and lower level files.
 */


#ifndef __WM_MANIPULATOR_TYPES_H__
#define __WM_MANIPULATOR_TYPES_H__

#include "BLI_compiler_attrs.h"

struct wmManipulatorMapType;
struct wmManipulatorGroupType;
struct wmManipulatorGroup;
struct wmManipulator;
struct wmManipulatorProperty;
struct wmKeyConfig;

#include "DNA_listBase.h"


/* -------------------------------------------------------------------- */
/* Enum Typedef's */


/**
 * #wmManipulator.state
 */
typedef enum eWM_ManipulatorState {
	WM_MANIPULATOR_STATE_HIGHLIGHT   = (1 << 0), /* while hovered */
	WM_MANIPULATOR_STATE_MODAL       = (1 << 1), /* while dragging */
	WM_MANIPULATOR_STATE_SELECT      = (1 << 2),
} eWM_ManipulatorState;


/**
 * #wmManipulator.flag
 * Flags for individual manipulators.
 */
typedef enum eWM_ManipulatorFlag {
	WM_MANIPULATOR_DRAW_HOVER  = (1 << 0), /* draw *only* while hovering */
	WM_MANIPULATOR_DRAW_MODAL  = (1 << 1), /* draw while dragging */
	WM_MANIPULATOR_DRAW_VALUE  = (1 << 2), /* draw an indicator for the current value while dragging */
	WM_MANIPULATOR_HIDDEN      = (1 << 3),
	/**
	 * When set 'scale_final' value also scales the offset.
	 * Use when offset is to avoid screen-space overlap instead of absolute positioning. */
	WM_MANIPULATOR_DRAW_OFFSET_SCALE  = (1 << 4),
	/**
	 * User should still use 'scale_final' for any handles and UI elements.
	 * This simply skips scale when calculating the final matrix.
	 * Needed when the manipulator needs to align with the interface underneath it. */
	WM_MANIPULATOR_DRAW_NO_SCALE  = (1 << 5),
	/**
	 * Hide the cursor and lock it's position while interacting with this manipulator.
	 */
	WM_MANIPULATOR_GRAB_CURSOR = (1 << 6),
} eWM_ManipulatorFlag;

/**
 * #wmManipulatorGroupType.flag
 * Flags that influence the behavior of all manipulators in the group.
 */
typedef enum eWM_ManipulatorGroupTypeFlag {
	/* Mark manipulator-group as being 3D */
	WM_MANIPULATORGROUPTYPE_3D       = (1 << 0),
	/* Scale manipulators as 3D object that respects zoom (otherwise zoom independent draw size).
	 * note: currently only for 3D views, 2D support needs adding. */
	WM_MANIPULATORGROUPTYPE_SCALE    = (1 << 1),
	/* Manipulators can be depth culled with scene objects (covered by other geometry - TODO) */
	WM_MANIPULATORGROUPTYPE_DEPTH_3D = (1 << 2),
	/* Manipulators can be selected */
	WM_MANIPULATORGROUPTYPE_SELECT  = (1 << 3),
	/* The manipulator group is to be kept (not removed on loading a new file for eg). */
	WM_MANIPULATORGROUPTYPE_PERSISTENT = (1 << 4),
	/* Show all other manipulators when interacting. */
	WM_MANIPULATORGROUPTYPE_DRAW_MODAL_ALL = (1 << 5),
} eWM_ManipulatorGroupTypeFlag;


/**
 * #wmManipulatorGroup.init_flag
 */
typedef enum eWM_ManipulatorGroupInitFlag {
	/* mgroup has been initialized */
	WM_MANIPULATORGROUP_INIT_SETUP = (1 << 0),
	WM_MANIPULATORGROUP_INIT_REFRESH = (1 << 1),
} eWM_ManipulatorGroupInitFlag;

/**
 * #wmManipulatorMapType.type_update_flag
 * Manipulator-map type update flag
 */
typedef enum eWM_ManipulatorMapTypeUpdateFlag {
	/* A new type has been added, needs to be initialized for all views. */
	WM_MANIPULATORMAPTYPE_UPDATE_INIT = (1 << 0),
	WM_MANIPULATORMAPTYPE_UPDATE_REMOVE = (1 << 1),

	/* Needed because keymap may be registered before and after window initialization.
	 * So we need to keep track of keymap initialization separately. */
	WM_MANIPULATORMAPTYPE_KEYMAP_INIT = (1 << 2),
} eWM_ManipulatorMapTypeUpdateFlag;

/* -------------------------------------------------------------------- */
/* wmManipulator */

/**
 * \brief Manipulator tweak flag.
 * Bitflag passed to manipulator while tweaking.
 *
 * \note Manipulators are responsible for handling this #wmManipulator.modal callback!.
 */
typedef enum {
	/* Drag with extra precision (Shift). */
	WM_MANIPULATOR_TWEAK_PRECISE = (1 << 0),
	/* Drag with snap enabled (Ctrl).  */
	WM_MANIPULATOR_TWEAK_SNAP = (1 << 1),
} eWM_ManipulatorTweak;

#include "wm_manipulator_fn.h"

typedef struct wmManipulatorOpElem {
	struct wmOperatorType *type;
	/* operator properties if manipulator spawns and controls an operator,
	 * or owner pointer if manipulator spawns and controls a property */
	PointerRNA ptr;
} wmManipulatorOpElem;

/* manipulators are set per region by registering them on manipulator-maps */
struct wmManipulator {
	struct wmManipulator *next, *prev;

	/* While we don't have a real type, use this to put type-like vars. */
	const struct wmManipulatorType *type;

	/* Overrides 'type->modal' when set.
	 * Note that this is a workaround, remove if we can. */
	wmManipulatorFnModal custom_modal;

	/* pointer back to group this manipulator is in (just for quick access) */
	struct wmManipulatorGroup *parent_mgroup;

	void *py_instance;

	/* rna pointer to access properties */
	struct PointerRNA *ptr;

	/* flags that influence the behavior or how the manipulators are drawn */
	eWM_ManipulatorFlag flag;
	/* state flags (active, highlighted, selected) */
	eWM_ManipulatorState state;

	/* Optional ID for highlighting different parts of this manipulator.
	 * -1 when unset, otherwise a valid index. (Used as index to 'op_data'). */
	int highlight_part;
	/* For single click button manipulators, use a different part as a fallback, -1 when unused. */
	int drag_part;

	/* Transformation of the manipulator in 2d or 3d space.
	 * - Matrix axis are expected to be unit length (scale is applied after).
	 * - Behavior when axis aren't orthogonal depends on each manipulator.
	 * - Typically the +Z is the primary axis for manipulators to use.
	 * - 'matrix[3]' must be used for location,
	 *   besides this it's up to the manipulators internal code how the
	 *   rotation components are used for drawing and interaction.
	 */

	/* The space this manipulator is being modified in. */
	float matrix_space[4][4];
	/* Transformation of this manipulator. */
	float matrix_basis[4][4];
	/* custom offset from origin */
	float matrix_offset[4][4];
	/* runtime property, set the scale while drawing on the viewport */
	float scale_final;
	/* user defined scale, in addition to the original one */
	float scale_basis;
	/* user defined width for line drawing */
	float line_width;
	/* manipulator colors (uses default fallbacks if not defined) */
	float color[4], color_hi[4];

	/* data used during interaction */
	void *interaction_data;

	/* Operator to spawn when activating the manipulator (overrides property editing),
	 * an array of items (aligned with #wmManipulator.highlight_part). */
	wmManipulatorOpElem *op_data;
	int op_data_len;

	struct IDProperty *properties;

	/* over alloc target_properties after 'wmManipulatorType.struct_size' */
};

/* Similar to PropertyElemRNA, but has an identifier. */
typedef struct wmManipulatorProperty {
	const struct wmManipulatorPropertyType *type;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;


	/* Optional functions for converting to/from RNA  */
	struct {
		wmManipulatorPropertyFnGet value_get_fn;
		wmManipulatorPropertyFnSet value_set_fn;
		wmManipulatorPropertyFnRangeGet range_get_fn;
		wmManipulatorPropertyFnFree free_fn;
		void *user_data;
	} custom_func;
} wmManipulatorProperty;

typedef struct wmManipulatorPropertyType {
	struct wmManipulatorPropertyType *next, *prev;
	/* PropertyType, typically 'PROP_FLOAT' */
	int data_type;
	int array_length;

	/* index within 'wmManipulatorType' */
	int index_in_type;

	/* over alloc */
	char idname[0];
} wmManipulatorPropertyType;


/**
 * Simple utility wrapper for storing a single manipulator as wmManipulatorGroup.customdata (which gets freed).
 */
typedef struct wmManipulatorWrapper {
	struct wmManipulator *manipulator;
} wmManipulatorWrapper;

struct wmManipulatorMapType_Params {
	short spaceid;
	short regionid;
};

typedef struct wmManipulatorType {

	const char *idname; /* MAX_NAME */

	/* Set to 'sizeof(wmManipulator)' or larger for instances of this type,
	 * use so we can cant to other types without the hassle of a custom-data pointer. */
	uint struct_size;

	/* Initialize struct (calloc'd 'struct_size' region). */
	wmManipulatorFnSetup setup;

	/* draw manipulator */
	wmManipulatorFnDraw draw;

	/* determines 3d intersection by rendering the manipulator in a selection routine. */
	wmManipulatorFnDrawSelect draw_select;

	/* Determine if the mouse intersects with the manipulator.
	 * The calculation should be done in the callback itself, -1 for no seleciton. */
	wmManipulatorFnTestSelect test_select;

	/* handler used by the manipulator. Usually handles interaction tied to a manipulator type */
	wmManipulatorFnModal modal;

	/* manipulator-specific handler to update manipulator attributes based on the property value */
	wmManipulatorFnPropertyUpdate property_update;

	/* Returns the final transformation which may be different from the 'matrix',
	 * depending on the manipulator.
	 * Notes:
	 * - Scale isn't applied (wmManipulator.scale/user_scale).
	 * - Offset isn't applied (wmManipulator.matrix_offset).
	 */
	wmManipulatorFnMatrixBasisGet matrix_basis_get;

	/* activate a manipulator state when the user clicks on it */
	wmManipulatorFnInvoke invoke;

	/* called when manipulator tweaking is done - used to free data and reset property when cancelling */
	wmManipulatorFnExit exit;

	wmManipulatorFnCursorGet cursor_get;

	/* called when manipulator selection state changes */
	wmManipulatorFnSelectRefresh select_refresh;

	/* Free data (not the manipulator it's self), use when the manipulator allocates it's own members. */
	wmManipulatorFnFree free;

	/* RNA for properties */
	struct StructRNA *srna;

	/* RNA integration */
	ExtensionRNA ext;

	ListBase target_property_defs;
	int target_property_defs_len;

} wmManipulatorType;


/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

/* factory class for a manipulator-group type, gets called every time a new area is spawned */
typedef struct wmManipulatorGroupTypeRef {
	struct wmManipulatorGroupTypeRef *next, *prev;
	struct wmManipulatorGroupType *type;
} wmManipulatorGroupTypeRef;

/* factory class for a manipulator-group type, gets called every time a new area is spawned */
typedef struct wmManipulatorGroupType {
	const char *idname;  /* MAX_NAME */
	const char *name; /* manipulator-group name - displayed in UI (keymap editor) */

	/* poll if manipulator-map should be visible */
	wmManipulatorGroupFnPoll poll;
	/* initially create manipulators and set permanent data - stuff you only need to do once */
	wmManipulatorGroupFnInit setup;
	/* refresh data, only called if recreate flag is set (WM_manipulatormap_tag_refresh) */
	wmManipulatorGroupFnRefresh refresh;
	/* refresh data for drawing, called before each redraw */
	wmManipulatorGroupFnDrawPrepare draw_prepare;

	/* Keymap init callback for this manipulator-group (optional),
	 * will fall back to default tweak keymap when left NULL. */
	wmManipulatorGroupFnSetupKeymap setup_keymap;

	/* Optionally subscribe to wmMsgBus events,
	 * these are calculated automatically from RNA properties,
	 * only needed if manipulators depend indirectly on properties. */
	wmManipulatorGroupFnMsgBusSubscribe message_subscribe;

	/* keymap created with callback from above */
	struct wmKeyMap *keymap;
	/* Only for convenient removal. */
	struct wmKeyConfig *keyconf;

	/* Disable for now, maybe some day we want properties. */
#if 0
	/* rna for properties */
	struct StructRNA *srna;
#endif

	/* RNA integration */
	ExtensionRNA ext;

	eWM_ManipulatorGroupTypeFlag flag;

	/* So we know which group type to update. */
	eWM_ManipulatorMapTypeUpdateFlag type_update_flag;

	/* same as manipulator-maps, so registering/unregistering goes to the correct region */
	struct wmManipulatorMapType_Params mmap_params;

} wmManipulatorGroupType;

typedef struct wmManipulatorGroup {
	struct wmManipulatorGroup *next, *prev;

	struct wmManipulatorGroupType *type;
	ListBase manipulators;

	struct wmManipulatorMap *parent_mmap;

	void *py_instance;            /* python stores the class instance here */
	struct ReportList *reports;   /* errors and warnings storage */

	void *customdata;
	void (*customdata_free)(void *); /* for freeing customdata from above */
	eWM_ManipulatorGroupInitFlag init_flag;
} wmManipulatorGroup;

/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

/**
 * Pass a value of this enum to #WM_manipulatormap_draw to tell it what to draw.
 */
typedef enum eWM_ManipulatorMapDrawStep {
	/** Draw 2D manipulator-groups (#WM_MANIPULATORGROUPTYPE_3D not set). */
	WM_MANIPULATORMAP_DRAWSTEP_2D = 0,
	/** Draw 3D manipulator-groups (#WM_MANIPULATORGROUPTYPE_3D set). */
	WM_MANIPULATORMAP_DRAWSTEP_3D,
} eWM_ManipulatorMapDrawStep;
#define WM_MANIPULATORMAP_DRAWSTEP_MAX 2

#endif  /* __WM_MANIPULATOR_TYPES_H__ */
