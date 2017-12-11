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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/wm_manipulator_intern.h
 *  \ingroup wm
 */


#ifndef __WM_MANIPULATOR_INTERN_H__
#define __WM_MANIPULATOR_INTERN_H__

struct wmKeyConfig;
struct wmManipulatorMap;
struct ManipulatorGeomInfo;
struct GHashIterator;

#include "wm_manipulator_fn.h"

/* -------------------------------------------------------------------- */
/* wmManipulator */


bool wm_manipulator_select_set_ex(
        struct wmManipulatorMap *mmap, struct wmManipulator *mpr, bool select,
        bool use_array, bool use_callback);
bool wm_manipulator_select_and_highlight(bContext *C, struct wmManipulatorMap *mmap, struct wmManipulator *mpr);

void wm_manipulator_calculate_scale(struct wmManipulator *mpr, const bContext *C);
void wm_manipulator_update(struct wmManipulator *mpr, const bContext *C, const bool refresh_map);

int wm_manipulator_is_visible(struct wmManipulator *mpr);
enum {
	WM_MANIPULATOR_IS_VISIBLE_UPDATE = (1 << 0),
	WM_MANIPULATOR_IS_VISIBLE_DRAW = (1 << 1),
};

/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

enum {
	TWEAK_MODAL_CANCEL = 1,
	TWEAK_MODAL_CONFIRM,
	TWEAK_MODAL_PRECISION_ON,
	TWEAK_MODAL_PRECISION_OFF,
	TWEAK_MODAL_SNAP_ON,
	TWEAK_MODAL_SNAP_OFF,
};

struct wmManipulatorGroup *wm_manipulatorgroup_new_from_type(
        struct wmManipulatorMap *mmap, struct wmManipulatorGroupType *wgt);
void wm_manipulatorgroup_free(bContext *C, struct wmManipulatorGroup *mgroup);
void wm_manipulatorgroup_manipulator_register(struct wmManipulatorGroup *mgroup, struct wmManipulator *mpr);
struct wmManipulator *wm_manipulatorgroup_find_intersected_mainpulator(
        const struct wmManipulatorGroup *mgroup, struct bContext *C, const struct wmEvent *event,
        int *r_part);
void wm_manipulatorgroup_intersectable_manipulators_to_list(
        const struct wmManipulatorGroup *mgroup, struct ListBase *listbase);
void wm_manipulatorgroup_ensure_initialized(struct wmManipulatorGroup *mgroup, const struct bContext *C);
bool wm_manipulatorgroup_is_visible(const struct wmManipulatorGroup *mgroup, const struct bContext *C);
bool wm_manipulatorgroup_is_visible_in_drawstep(
        const struct wmManipulatorGroup *mgroup, const eWM_ManipulatorMapDrawStep drawstep);

void wm_manipulatorgrouptype_setup_keymap(
        struct wmManipulatorGroupType *wgt, struct wmKeyConfig *keyconf);


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

typedef struct wmManipulatorMapSelectState {
	struct wmManipulator **items;
	int len, len_alloc;
} wmManipulatorMapSelectState;

struct wmManipulatorMap {

	struct wmManipulatorMapType *type;
	ListBase groups;  /* wmManipulatorGroup */

	/* private, update tagging (enum defined in C source). */
	char update_flag[WM_MANIPULATORMAP_DRAWSTEP_MAX];

	/**
	 * \brief Manipulator map runtime context
	 *
	 * Contains information about this manipulator-map. Currently
	 * highlighted manipulator, currently selected manipulators, ...
	 */
	struct {
		/* we redraw the manipulator-map when this changes */
		struct wmManipulator *highlight;
		/* User has clicked this manipulator and it gets all input. */
		struct wmManipulator *modal;
		/* array for all selected manipulators */
		struct wmManipulatorMapSelectState select;
		/* cursor location at point of entering modal (see: WM_MANIPULATOR_GRAB_CURSOR) */
		int event_xy[2];
		short event_grabcursor;
	} mmap_context;
};

/**
 * This is a container for all manipulator types that can be instantiated in a region.
 * (similar to dropboxes).
 *
 * \note There is only ever one of these for every (area, region) combination.
 */
struct wmManipulatorMapType {
	struct wmManipulatorMapType *next, *prev;
	short spaceid, regionid;
	/* types of manipulator-groups for this manipulator-map type */
	ListBase grouptype_refs;

	/* eManipulatorMapTypeUpdateFlags */
	eWM_ManipulatorMapTypeUpdateFlag type_update_flag;
};

void wm_manipulatormap_select_array_clear(struct wmManipulatorMap *mmap);
bool wm_manipulatormap_deselect_all(struct wmManipulatorMap *mmap);
void wm_manipulatormap_select_array_shrink(struct wmManipulatorMap *mmap, int len_subtract);
void wm_manipulatormap_select_array_push_back(struct wmManipulatorMap *mmap, wmManipulator *mpr);
void wm_manipulatormap_select_array_remove(struct wmManipulatorMap *mmap, wmManipulator *mpr);

#endif
