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

struct wmManipulatorGroupType;
struct wmManipulatorGroup;
struct wmKeyConfig;

typedef bool (*wmManipulatorGroupPollFunc)(const struct bContext *, struct wmManipulatorGroupType *) ATTR_WARN_UNUSED_RESULT;
typedef void (*wmManipulatorGroupInitFunc)(const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupRefreshFunc)(const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupDrawPrepareFunc)(const struct bContext *, struct wmManipulatorGroup *);


/* -------------------------------------------------------------------- */
/* wmManipulator */

/**
 * Simple utility wrapper for storing a single manipulator as wmManipulatorGroup.customdata (which gets freed).
 */
typedef struct wmManipulatorWrapper {
	struct wmManipulator *manipulator;
} wmManipulatorWrapper;

/* wmManipulator.flag
 * Flags for individual manipulators. */
enum {
	WM_MANIPULATOR_DRAW_HOVER  = (1 << 0), /* draw *only* while hovering */
	WM_MANIPULATOR_DRAW_ACTIVE = (1 << 1), /* draw while dragging */
	WM_MANIPULATOR_DRAW_VALUE  = (1 << 2), /* draw an indicator for the current value while dragging */
	WM_MANIPULATOR_HIDDEN      = (1 << 3),
};


/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

typedef struct wmManipulatorGroup {
	struct wmManipulatorGroup *next, *prev;

	struct wmManipulatorGroupType *type;
	ListBase manipulators;

	void *py_instance;            /* python stores the class instance here */
	struct ReportList *reports;   /* errors and warnings storage */

	void *customdata;
	void (*customdata_free)(void *); /* for freeing customdata from above */
	int flag; /* private */
	int pad;
} wmManipulatorGroup;

/* factory class for a manipulator-group type, gets called every time a new area is spawned */
typedef struct wmManipulatorGroupType {
	struct wmManipulatorGroupType *next, *prev;

	char idname[64];  /* MAX_NAME */
	const char *name; /* manipulator-group name - displayed in UI (keymap editor) */

	/* poll if manipulator-map should be visible */
	wmManipulatorGroupPollFunc poll;
	/* initially create manipulators and set permanent data - stuff you only need to do once */
	wmManipulatorGroupInitFunc init;
	/* refresh data, only called if recreate flag is set (WM_manipulatormap_tag_refresh) */
	wmManipulatorGroupRefreshFunc refresh;
	/* refresh data for drawing, called before each redraw */
	wmManipulatorGroupDrawPrepareFunc draw_prepare;

	/* keymap init callback for this manipulator-group */
	struct wmKeyMap *(*keymap_init)(const struct wmManipulatorGroupType *, struct wmKeyConfig *);
	/* keymap created with callback from above */
	struct wmKeyMap *keymap;

	/* rna for properties */
	struct StructRNA *srna;

	/* RNA integration */
	ExtensionRNA ext;

	int flag;

	/* if type is spawned from operator this is set here */
	void *op;

	/* same as manipulator-maps, so registering/unregistering goes to the correct region */
	short spaceid, regionid;
	char mapidname[64];
} wmManipulatorGroupType;

/**
 * wmManipulatorGroupType.flag
 * Flags that influence the behavior of all manipulators in the group.
 */
enum {
	/* Mark manipulator-group as being 3D */
	WM_MANIPULATORGROUPTYPE_IS_3D       = (1 << 0),
	/* Scale manipulators as 3D object that respects zoom (otherwise zoom independent draw size) */
	WM_MANIPULATORGROUPTYPE_SCALE_3D    = (1 << 1),
	/* Manipulators can be depth culled with scene objects (covered by other geometry - TODO) */
	WM_MANIPULATORGROUPTYPE_SCENE_DEPTH = (1 << 2),
	/* Manipulators can be selected */
	WM_MANIPULATORGROUPTYPE_SELECTABLE  = (1 << 3),
	/* manipulator group is attached to operator, and is only accessible as long as this runs */
	WM_MANIPULATORGROUPTYPE_OP          = (1 << 4),
};


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

struct wmManipulatorMapType_Params {
	const char *idname;
	const int spaceid;
	const int regionid;
};

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

