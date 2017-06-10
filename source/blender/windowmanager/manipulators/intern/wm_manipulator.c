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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/wm_manipulator.c
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_manipulator_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_batch.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* only for own init/exit calls (wm_manipulatortype_init/wm_manipulatortype_free) */
#include "wm.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

static void wm_manipulator_register(
        wmManipulatorGroup *mgroup, wmManipulator *mpr, const char *name);

/** \name Manipulator Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

static GHash *global_manipulatortype_hash = NULL;

const wmManipulatorType *WM_manipulatortype_find(const char *idname, bool quiet)
{
	if (idname[0]) {
		wmManipulatorType *wt;

		wt = BLI_ghash_lookup(global_manipulatortype_hash, idname);
		if (wt) {
			return wt;
		}

		if (!quiet) {
			printf("search for unknown manipulator '%s'\n", idname);
		}
	}
	else {
		if (!quiet) {
			printf("search for empty manipulator\n");
		}
	}

	return NULL;
}

/* caller must free */
void WM_manipulatortype_iter(GHashIterator *ghi)
{
	BLI_ghashIterator_init(ghi, global_manipulatortype_hash);
}

static wmManipulatorType *wm_manipulatortype_append__begin(void)
{
	wmManipulatorType *wt = MEM_callocN(sizeof(wmManipulatorType), "manipulatortype");
	return wt;
}
static void wm_manipulatortype_append__end(wmManipulatorType *wt)
{
	BLI_assert(wt->struct_size >= sizeof(wmManipulator));

	BLI_ghash_insert(global_manipulatortype_hash, (void *)wt->idname, wt);
}

void WM_manipulatortype_append(void (*wtfunc)(struct wmManipulatorType *))
{
	wmManipulatorType *wt = wm_manipulatortype_append__begin();
	wtfunc(wt);
	wm_manipulatortype_append__end(wt);
}

void WM_manipulatortype_append_ptr(void (*wtfunc)(struct wmManipulatorType *, void *), void *userdata)
{
	wmManipulatorType *mt = wm_manipulatortype_append__begin();
	wtfunc(mt, userdata);
	wm_manipulatortype_append__end(mt);
}

/**
 * Free but don't remove from ghash.
 */
static void manipulatortype_free(wmManipulatorType *wt)
{
	MEM_freeN(wt);
}

void WM_manipulatortype_remove_ptr(wmManipulatorType *wt)
{
	BLI_assert(wt == WM_manipulatortype_find(wt->idname, false));

	BLI_ghash_remove(global_manipulatortype_hash, wt->idname, NULL, NULL);

	manipulatortype_free(wt);
}

bool WM_manipulatortype_remove(const char *idname)
{
	wmManipulatorType *wt = BLI_ghash_lookup(global_manipulatortype_hash, idname);

	if (wt == NULL) {
		return false;
	}

	WM_manipulatortype_remove_ptr(wt);

	return true;
}

static void wm_manipulatortype_ghash_free_cb(wmManipulatorType *mt)
{
	manipulatortype_free(mt);
}

void wm_manipulatortype_free(void)
{
	BLI_ghash_free(global_manipulatortype_hash, NULL, (GHashValFreeFP)wm_manipulatortype_ghash_free_cb);
	global_manipulatortype_hash = NULL;
}

/* called on initialize WM_init() */
void wm_manipulatortype_init(void)
{
	/* reserve size is set based on blender default setup */
	global_manipulatortype_hash = BLI_ghash_str_new_ex("wm_manipulatortype_init gh", 128);
}

/** \} */

/**
 * \note Follow #wm_operator_create convention.
 */
static wmManipulator *wm_manipulator_create(
        const wmManipulatorType *mpt)
{
	BLI_assert(mpt != NULL);
	BLI_assert(mpt->struct_size >= sizeof(wmManipulator));

	wmManipulator *mpr = MEM_callocN(mpt->struct_size, __func__);
	mpr->type = mpt;
	return mpr;
}

wmManipulator *WM_manipulator_new_ptr(const wmManipulatorType *mpt, wmManipulatorGroup *mgroup, const char *name)
{
	wmManipulator *mpr = wm_manipulator_create(mpt);

	wm_manipulator_register(mgroup, mpr, name);

	return mpr;
}

/**
 * \param wt: Must be valid,
 * if you need to check it exists use #WM_manipulator_new_ptr
 * because callers of this function don't NULL check the return value.
 */
wmManipulator *WM_manipulator_new(const char *idname, wmManipulatorGroup *mgroup, const char *name)
{
	const wmManipulatorType *wt = WM_manipulatortype_find(idname, false);
	wmManipulator *mpr = wm_manipulator_create(wt);

	wm_manipulator_register(mgroup, mpr, name);

	return mpr;
}

wmManipulatorGroup *WM_manipulator_get_parent_group(wmManipulator *mpr)
{
	return mpr->parent_mgroup;
}

/**
 * Assign an idname that is unique in \a mgroup to \a manipulator.
 *
 * \param rawname: Name used as basis to define final unique idname.
 */
static void manipulator_unique_idname_set(wmManipulatorGroup *mgroup, wmManipulator *mpr, const char *rawname)
{
	BLI_snprintf(mpr->name, sizeof(mpr->name), "%s_%s", mgroup->type->idname, rawname);

	/* ensure name is unique, append '.001', '.002', etc if not */
	BLI_uniquename(&mgroup->manipulators, mpr, "Manipulator", '.',
	               offsetof(wmManipulator, name), sizeof(mpr->name));
}

/**
 * Initialize default values and allocate needed memory for members.
 */
static void manipulator_init(wmManipulator *mpr)
{
	const float color_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	mpr->user_scale = 1.0f;
	mpr->line_width = 1.0f;

	/* defaults */
	copy_v4_v4(mpr->color, color_default);
	copy_v4_v4(mpr->color_hi, color_default);
}

/**
 * Register \a manipulator.
 *
 * \param name: name used to create a unique idname for \a manipulator in \a mgroup
 *
 * \note Not to be confused with type registration from RNA.
 */
static void wm_manipulator_register(wmManipulatorGroup *mgroup, wmManipulator *mpr, const char *name)
{
	manipulator_init(mpr);
	manipulator_unique_idname_set(mgroup, mpr, name);
	wm_manipulatorgroup_manipulator_register(mgroup, mpr);
}

/**
 * Free \a manipulator and unlink from \a manipulatorlist.
 * \a manipulatorlist is allowed to be NULL.
 */
void WM_manipulator_free(ListBase *manipulatorlist, wmManipulatorMap *mmap, wmManipulator *mpr, bContext *C)
{
#ifdef WITH_PYTHON
	if (mpr->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(mpr->py_instance);
	}
#endif

	if (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) {
		wm_manipulatormap_highlight_set(mmap, C, NULL, 0);
	}
	if (mpr->state & WM_MANIPULATOR_STATE_ACTIVE) {
		wm_manipulatormap_active_set(mmap, C, NULL, NULL);
	}
	if (mpr->state & WM_MANIPULATOR_STATE_SELECT) {
		wm_manipulator_deselect(mmap, mpr);
	}

	if (mpr->opptr.data) {
		WM_operator_properties_free(&mpr->opptr);
	}
	BLI_freelistN(&mpr->properties);

	if (manipulatorlist)
		BLI_remlink(manipulatorlist, mpr);
	MEM_freeN(mpr);
}

wmManipulatorGroup *wm_manipulator_get_parent_group(const wmManipulator *mpr)
{
	return mpr->parent_mgroup;
}


/* -------------------------------------------------------------------- */
/** \name Manipulator Creation API
 *
 * API for defining data on manipulator creation.
 *
 * \{ */

struct wmManipulatorProperty *WM_manipulator_get_property(wmManipulator *mpr, const char *idname)
{
	return BLI_findstring(&mpr->properties, idname, offsetof(wmManipulatorProperty, idname));
}

void WM_manipulator_def_property(
        wmManipulator *mpr, const char *idname,
        PointerRNA *ptr, const char *propname, int index)
{
	wmManipulatorProperty *mpr_prop = WM_manipulator_get_property(mpr, idname);

	if (mpr_prop == NULL) {
		const uint idname_size = strlen(idname) + 1;
		mpr_prop = MEM_callocN(sizeof(wmManipulatorProperty) + idname_size, __func__);
		memcpy(mpr_prop->idname, idname, idname_size);
		BLI_addtail(&mpr->properties, mpr_prop);
	}

	/* if manipulator evokes an operator we cannot use it for property manipulation */
	mpr->opname = NULL;
	mpr_prop->ptr = *ptr;
	mpr_prop->prop = RNA_struct_find_property(ptr, propname);
	mpr_prop->index = index;

	if (mpr->type->property_update) {
		mpr->type->property_update(mpr, mpr_prop);
	}
}

PointerRNA *WM_manipulator_set_operator(wmManipulator *mpr, const char *opname)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);

	if (ot) {
		mpr->opname = opname;

		if (mpr->opptr.data) {
			WM_operator_properties_free(&mpr->opptr);
		}
		WM_operator_properties_create_ptr(&mpr->opptr, ot);

		return &mpr->opptr;
	}
	else {
		fprintf(stderr, "Error binding operator to manipulator: operator %s not found!\n", opname);
	}

	return NULL;
}

void WM_manipulator_set_origin(wmManipulator *mpr, const float origin[3])
{
	copy_v3_v3(mpr->origin, origin);
}

void WM_manipulator_set_offset(wmManipulator *mpr, const float offset[3])
{
	copy_v3_v3(mpr->offset, offset);
}

void WM_manipulator_set_flag(wmManipulator *mpr, const int flag, const bool enable)
{
	if (enable) {
		mpr->flag |= flag;
	}
	else {
		mpr->flag &= ~flag;
	}
}

void WM_manipulator_set_scale(wmManipulator *mpr, const float scale)
{
	mpr->user_scale = scale;
}

void WM_manipulator_set_line_width(wmManipulator *mpr, const float line_width)
{
	mpr->line_width = line_width;
}

/**
 * Set manipulator rgba colors.
 *
 * \param col  Normal state color.
 * \param col_hi  Highlighted state color.
 */
void WM_manipulator_get_color(const wmManipulator *mpr, float col[4])
{
	copy_v4_v4(col, mpr->color);
}
void WM_manipulator_set_color(wmManipulator *mpr, const float col[4])
{
	copy_v4_v4(mpr->color, col);
}

void WM_manipulator_get_color_highlight(const wmManipulator *mpr, float color_hi[4])
{
	copy_v4_v4(color_hi, mpr->color_hi);
}
void WM_manipulator_set_color_highlight(wmManipulator *mpr, const float color_hi[4])
{
	copy_v4_v4(mpr->color_hi, color_hi);
}


/** \} */ // Manipulator Creation API


/* -------------------------------------------------------------------- */
/** \name Manipulator Callback Assignment
 *
 * \{ */

void WM_manipulator_set_fn_custom_modal(struct wmManipulator *mpr, wmManipulatorFnModal fn)
{
	mpr->custom_modal = fn;
}

/** \} */


/* -------------------------------------------------------------------- */

/**
 * Remove \a manipulator from selection.
 * Reallocates memory for selected manipulators so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_manipulator_deselect(wmManipulatorMap *mmap, wmManipulator *mpr)
{
	if (!mmap->mmap_context.selected)
		return false;

	wmManipulator ***sel = &mmap->mmap_context.selected;
	int *selected_len = &mmap->mmap_context.selected_len;
	bool changed = false;

	/* caller should check! */
	BLI_assert(mpr->state & WM_MANIPULATOR_STATE_SELECT);

	/* remove manipulator from selected_manipulators array */
	for (int i = 0; i < (*selected_len); i++) {
		if ((*sel)[i] == mpr) {
			for (int j = i; j < ((*selected_len) - 1); j++) {
				(*sel)[j] = (*sel)[j + 1];
			}
			changed = true;
			break;
		}
	}

	/* update array data */
	if ((*selected_len) <= 1) {
		wm_manipulatormap_selected_clear(mmap);
	}
	else {
		*sel = MEM_reallocN(*sel, sizeof(**sel) * (*selected_len));
		(*selected_len)--;
	}

	mpr->state &= ~WM_MANIPULATOR_STATE_SELECT;
	return changed;
}

/**
 * Add \a manipulator to selection.
 * Reallocates memory for selected manipulators so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_manipulator_select(bContext *C, wmManipulatorMap *mmap, wmManipulator *mpr)
{
	wmManipulator ***sel = &mmap->mmap_context.selected;
	int *selected_len = &mmap->mmap_context.selected_len;

	if (!mpr || (mpr->state & WM_MANIPULATOR_STATE_SELECT))
		return false;

	(*selected_len)++;

	*sel = MEM_reallocN(*sel, sizeof(wmManipulator *) * (*selected_len));
	(*sel)[(*selected_len) - 1] = mpr;

	mpr->state |= WM_MANIPULATOR_STATE_SELECT;
	if (mpr->type->select) {
		mpr->type->select(C, mpr, SEL_SELECT);
	}
	wm_manipulatormap_highlight_set(mmap, C, mpr, mpr->highlight_part);

	return true;
}

void wm_manipulator_calculate_scale(wmManipulator *mpr, const bContext *C)
{
	const RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float scale = 1.0f;

	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SCALE_3D) {
		if (rv3d /*&& (U.manipulator_flag & V3D_DRAW_MANIPULATOR) == 0*/) { /* UserPref flag might be useful for later */
			if (mpr->type->position_get) {
				float position[3];

				mpr->type->position_get(mpr, position);
				scale = ED_view3d_pixel_size(rv3d, position) * (float)U.manipulator_scale;
			}
			else {
				scale = ED_view3d_pixel_size(rv3d, mpr->origin) * (float)U.manipulator_scale;
			}
		}
		else {
			scale = U.manipulator_scale * 0.02f;
		}
	}

	mpr->scale = scale * mpr->user_scale;
}

static void manipulator_update_prop_data(wmManipulator *mpr)
{
	/* manipulator property might have been changed, so update manipulator */
	if (mpr->type->property_update && !BLI_listbase_is_empty(&mpr->properties)) {
		for (wmManipulatorProperty *mpr_prop = mpr->properties.first; mpr_prop; mpr_prop = mpr_prop->next) {
			if (mpr_prop->prop != NULL) {
				mpr->type->property_update(mpr, mpr_prop);
			}
		}
	}
}

void wm_manipulator_update(wmManipulator *mpr, const bContext *C, const bool refresh_map)
{
	if (refresh_map) {
		manipulator_update_prop_data(mpr);
	}
	wm_manipulator_calculate_scale(mpr, C);
}

bool wm_manipulator_is_visible(wmManipulator *mpr)
{
	if (mpr->flag & WM_MANIPULATOR_HIDDEN) {
		return false;
	}
	if ((mpr->state & WM_MANIPULATOR_STATE_ACTIVE) &&
	    !(mpr->flag & (WM_MANIPULATOR_DRAW_ACTIVE | WM_MANIPULATOR_DRAW_VALUE)))
	{
		/* don't draw while active (while dragging) */
		return false;
	}
	if ((mpr->flag & WM_MANIPULATOR_DRAW_HOVER) &&
	    !(mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) &&
	    !(mpr->state & WM_MANIPULATOR_STATE_SELECT)) /* still draw selected manipulators */
	{
		/* only draw on mouse hover */
		return false;
	}

	return true;
}
