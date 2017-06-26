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

#include "wm_manipulator_fn.h"

#include "DNA_listBase.h"

/* -------------------------------------------------------------------- */
/* wmManipulator */

/* manipulators are set per region by registering them on manipulator-maps */
struct wmManipulator {
	struct wmManipulator *next, *prev;

	char name[64 + 4]; /* MAX_NAME + 4 for unique '.001', '.002', etc suffix */

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

	int flag; /* flags that influence the behavior or how the manipulators are drawn */
	short state; /* state flags (active, highlighted, selected) */

	/* Optional ID for highlighting different parts of this manipulator. */
	int highlight_part;

	/* Transformation of the manipulator in 2d or 3d space.
	 * - Matrix axis are expected to be unit length (scale is applied after).
	 * - Behavior when axis aren't orthogonal depends on each manipulator.
	 * - Typically the +Z is the primary axis for manipulators to use.
	 * - 'matrix[3]' must be used for location,
	 *   besides this it's up to the manipulators internal code how the
	 *   rotation components are used for drawing and interaction.
	 */
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

	/* name of operator to spawn when activating the manipulator */
	struct {
		struct wmOperatorType *type;
		/* operator properties if manipulator spawns and controls an operator,
		 * or owner pointer if manipulator spawns and controls a property */
		PointerRNA ptr;
	} op_data;

	struct IDProperty *properties;

	/* over alloc target_properties after 'wmManipulatorType.struct_size' */
};

typedef void (*wmManipulatorGroupFnInit)(
        const struct bContext *, struct wmManipulatorGroup *);

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
		const struct bContext *context;
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


/* wmManipulator.flag
 * Flags for individual manipulators. */
enum {
	WM_MANIPULATOR_DRAW_HOVER  = (1 << 0), /* draw *only* while hovering */
	WM_MANIPULATOR_DRAW_ACTIVE = (1 << 1), /* draw while dragging */
	WM_MANIPULATOR_DRAW_VALUE  = (1 << 2), /* draw an indicator for the current value while dragging */
	WM_MANIPULATOR_HIDDEN      = (1 << 3),
};

/* wmManipulator.state */
enum {
	WM_MANIPULATOR_STATE_HIGHLIGHT   = (1 << 0), /* while hovered */
	WM_MANIPULATOR_STATE_ACTIVE      = (1 << 1), /* while dragging */
	WM_MANIPULATOR_STATE_SELECT      = (1 << 2),
};

/**
 * \brief Manipulator tweak flag.
 * Bitflag passed to manipulator while tweaking.
 *
 * \note Manipulators are responsible for handling this #wmManipulator.modal callback!.
 */
enum {
	/* Drag with extra precision (Shift). */
	WM_MANIPULATOR_TWEAK_PRECISE = (1 << 0),
	/* Drag with snap enabled (Ctrl).  */
	WM_MANIPULATOR_TWEAK_SNAP = (1 << 1),
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

	/* determine if the mouse intersects with the manipulator. The calculation should be done in the callback itself */
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
	wmManipulatorFnMatrixWorldGet matrix_world_get;

	/* activate a manipulator state when the user clicks on it */
	wmManipulatorFnInvoke invoke;

	/* called when manipulator tweaking is done - used to free data and reset property when cancelling */
	wmManipulatorFnExit exit;

	wmManipulatorFnCursorGet cursor_get;

	/* called when manipulator selection state changes */
	wmManipulatorFnSelect select;

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

	int flag;

	/* eManipulatorMapTypeUpdateFlags (so we know which group type to update) */
	uchar type_update_flag;

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
	int flag; /* private */
	int pad;
} wmManipulatorGroup;

/**
 * Manipulator-map type update flag: `wmManipulatorMapType.type_update_flag`
 */
enum eManipulatorMapTypeUpdateFlags {
	/* A new type has been added, needs to be initialized for all views. */
	WM_MANIPULATORMAPTYPE_UPDATE_INIT = (1 << 0),
	WM_MANIPULATORMAPTYPE_UPDATE_REMOVE = (1 << 1),
};

/**
 * wmManipulatorGroupType.flag
 * Flags that influence the behavior of all manipulators in the group.
 */
enum {
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
};


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

/**
 * Pass a value of this enum to #WM_manipulatormap_draw to tell it what to draw.
 */
enum {
	/* Draw 2D manipulator-groups (ManipulatorGroupType.is_3d == false) */
	WM_MANIPULATORMAP_DRAWSTEP_2D = 0,
	/* Draw 3D manipulator-groups (ManipulatorGroupType.is_3d == true) */
	WM_MANIPULATORMAP_DRAWSTEP_3D,
	/* Draw only depth culled manipulators (WM_MANIPULATOR_SCENE_DEPTH flag).
	 * Note that these are expected to be 3D manipulators too. */
	WM_MANIPULATORMAP_DRAWSTEP_IN_SCENE,
};

#endif  /* __WM_MANIPULATOR_TYPES_H__ */
