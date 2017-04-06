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

/* -------------------------------------------------------------------- */
/* wmManipulator */

/* manipulators are set per region by registering them on manipulator-maps */
struct wmManipulator {
	struct wmManipulator *next, *prev;

	char idname[MAX_NAME + 4]; /* + 4 for unique '.001', '.002', etc suffix */
	/* pointer back to group this manipulator is in (just for quick access) */
	struct wmManipulatorGroup *mgroup;

	/* could become wmManipulatorType */
	/* draw manipulator */
	void (*draw)(const struct bContext *, struct wmManipulator *);

	/* determine if the mouse intersects with the manipulator. The calculation should be done in the callback itself */
	int  (*intersect)(struct bContext *, const struct wmEvent *, struct wmManipulator *);

	/* determines 3d intersection by rendering the manipulator in a selection routine. */
	void (*render_3d_intersection)(const struct bContext *, struct wmManipulator *, int);

	/* handler used by the manipulator. Usually handles interaction tied to a manipulator type */
	int  (*handler)(struct bContext *, const struct wmEvent *, struct wmManipulator *, const int);

	/* manipulator-specific handler to update manipulator attributes based on the property value */
	void (*prop_data_update)(struct wmManipulator *, int);

	/* returns the final position which may be different from the origin, depending on the manipulator.
	 * used in calculations of scale */
	void (*get_final_position)(struct wmManipulator *, float[]);

	/* activate a manipulator state when the user clicks on it */
	int (*invoke)(struct bContext *, const struct wmEvent *, struct wmManipulator *);

	/* called when manipulator tweaking is done - used to free data and reset property when cancelling */
	void (*exit)(struct bContext *, struct wmManipulator *, const bool );

	int (*get_cursor)(struct wmManipulator *);

	/* called when manipulator selection state changes */
	wmManipulatorSelectFunc select;

	int flag; /* flags that influence the behavior or how the manipulators are drawn */
	short state; /* state flags (active, highlighted, selected) */

	unsigned char highlighted_part;

	/* center of manipulator in space, 2d or 3d */
	float origin[3];
	/* custom offset from origin */
	float offset[3];
	/* runtime property, set the scale while drawing on the viewport */
	float scale;
	/* user defined scale, in addition to the original one */
	float user_scale;
	/* user defined width for line drawing */
	float line_width;
	/* manipulator colors (uses default fallbacks if not defined) */
	float col[4], col_hi[4];

	/* data used during interaction */
	void *interaction_data;

	/* name of operator to spawn when activating the manipulator */
	const char *opname;
	/* operator properties if manipulator spawns and controls an operator,
	 * or owner pointer if manipulator spawns and controls a property */
	PointerRNA opptr;

	/* maximum number of properties attached to the manipulator */
	int max_prop;
	/* arrays of properties attached to various manipulator parameters. As
	 * the manipulator is interacted with, those properties get updated */
	PointerRNA *ptr;
	PropertyRNA **props;
};

/* wmManipulator.state */
enum {
	WM_MANIPULATOR_HIGHLIGHT   = (1 << 0), /* while hovered */
	WM_MANIPULATOR_ACTIVE      = (1 << 1), /* while dragging */
	WM_MANIPULATOR_SELECTED    = (1 << 2),
};

/**
 * \brief Manipulator tweak flag.
 * Bitflag passed to manipulator while tweaking.
 */
enum {
	/* drag with extra precision (shift)
	 * NOTE: Manipulators are responsible for handling this (manipulator->handler callback)! */
	WM_MANIPULATOR_TWEAK_PRECISE = (1 << 0),
};

void wm_manipulator_register(struct wmManipulatorGroup *mgroup, struct wmManipulator *manipulator, const char *name);

bool wm_manipulator_deselect(struct wmManipulatorMap *mmap, struct wmManipulator *manipulator);
bool wm_manipulator_select(bContext *C, struct wmManipulatorMap *mmap, struct wmManipulator *manipulator);

void wm_manipulator_calculate_scale(struct wmManipulator *manipulator, const bContext *C);
void wm_manipulator_update(struct wmManipulator *manipulator, const bContext *C, const bool refresh_map);
bool wm_manipulator_is_visible(struct wmManipulator *manipulator);

void fix_linking_manipulator_arrow(void);
void fix_linking_manipulator_arrow2d(void);
void fix_linking_manipulator_cage(void);
void fix_linking_manipulator_dial(void);
void fix_linking_manipulator_facemap(void);
void fix_linking_manipulator_primitive(void);


/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

enum {
	TWEAK_MODAL_CANCEL = 1,
	TWEAK_MODAL_CONFIRM,
	TWEAK_MODAL_PRECISION_ON,
	TWEAK_MODAL_PRECISION_OFF,
};

struct wmManipulatorGroup *wm_manipulatorgroup_new_from_type(struct wmManipulatorGroupType *mgrouptype);
void wm_manipulatorgroup_free(bContext *C, struct wmManipulatorMap *mmap, struct wmManipulatorGroup *mgroup);
void wm_manipulatorgroup_manipulator_register(struct wmManipulatorGroup *mgroup, struct wmManipulator *manipulator);
struct wmManipulator *wm_manipulatorgroup_find_intersected_mainpulator(
        const struct wmManipulatorGroup *mgroup, struct bContext *C, const struct wmEvent *event,
        unsigned char *part);
void wm_manipulatorgroup_intersectable_manipulators_to_list(
        const struct wmManipulatorGroup *mgroup, struct ListBase *listbase);
void wm_manipulatorgroup_ensure_initialized(struct wmManipulatorGroup *mgroup, const struct bContext *C);
bool wm_manipulatorgroup_is_visible(const struct wmManipulatorGroup *mgroup, const struct bContext *C);
bool wm_manipulatorgroup_is_visible_in_drawstep(const struct wmManipulatorGroup *mgroup, const int drawstep);

void wm_manipulatorgrouptype_keymap_init(struct wmManipulatorGroupType *mgrouptype, struct wmKeyConfig *keyconf);


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

struct wmManipulatorMap {
	struct wmManipulatorMap *next, *prev;

	struct wmManipulatorMapType *type;
	ListBase manipulator_groups;

	char update_flag; /* private, update tagging */

	/**
	 * \brief Manipulator map runtime context
	 *
	 * Contains information about this manipulator-map. Currently
	 * highlighted manipulator, currently selected manipulators, ...
	 */
	struct {
		/* we redraw the manipulator-map when this changes */
		struct wmManipulator *highlighted_manipulator;
		/* user has clicked this manipulator and it gets all input */
		struct wmManipulator *active_manipulator;
		/* array for all selected manipulators
		 * TODO  check on using BLI_array */
		struct wmManipulator **selected_manipulator;
		int tot_selected;
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
	char idname[64];
	short spaceid, regionid;
	/* types of manipulator-groups for this manipulator-map type */
	ListBase manipulator_grouptypes;
};

void wm_manipulatormap_selected_delete(struct wmManipulatorMap *mmap);
bool wm_manipulatormap_deselect_all(struct wmManipulatorMap *mmap, struct wmManipulator ***sel);


/* -------------------------------------------------------------------- */
/* Manipulator drawing */

void wm_manipulator_geometryinfo_draw(const struct ManipulatorGeomInfo *info, const bool select, const float color[4]);

#endif  /* __WM_MANIPULATOR_INTERN_H__ */

