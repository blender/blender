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

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"

#include "GPU_batch.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_idprop.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* only for own init/exit calls (wm_manipulatortype_init/wm_manipulatortype_free) */
#include "wm.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

static void wm_manipulator_register(
        wmManipulatorGroup *mgroup, wmManipulator *mpr);

/**
 * \note Follow #wm_operator_create convention.
 */
static wmManipulator *wm_manipulator_create(
        const wmManipulatorType *wt,
        PointerRNA *properties)
{
	BLI_assert(wt != NULL);
	BLI_assert(wt->struct_size >= sizeof(wmManipulator));

	wmManipulator *mpr = MEM_callocN(
	        wt->struct_size + (sizeof(wmManipulatorProperty) * wt->target_property_defs_len), __func__);
	mpr->type = wt;

	/* initialize properties, either copy or create */
	mpr->ptr = MEM_callocN(sizeof(PointerRNA), "wmManipulatorPtrRNA");
	if (properties && properties->data) {
		mpr->properties = IDP_CopyProperty(properties->data);
	}
	else {
		IDPropertyTemplate val = {0};
		mpr->properties = IDP_New(IDP_GROUP, &val, "wmManipulatorProperties");
	}
	RNA_pointer_create(G.main->wm.first, wt->srna, mpr->properties, mpr->ptr);

	WM_manipulator_properties_sanitize(mpr->ptr, 0);

	unit_m4(mpr->matrix_space);
	unit_m4(mpr->matrix_basis);
	unit_m4(mpr->matrix_offset);

	mpr->drag_part = -1;

	return mpr;
}

wmManipulator *WM_manipulator_new_ptr(
        const wmManipulatorType *wt, wmManipulatorGroup *mgroup,
        PointerRNA *properties)
{
	wmManipulator *mpr = wm_manipulator_create(wt, properties);

	wm_manipulator_register(mgroup, mpr);

	if (mpr->type->setup != NULL) {
		mpr->type->setup(mpr);
	}

	return mpr;
}

/**
 * \param wt: Must be valid,
 * if you need to check it exists use #WM_manipulator_new_ptr
 * because callers of this function don't NULL check the return value.
 */
wmManipulator *WM_manipulator_new(
        const char *idname, wmManipulatorGroup *mgroup,
        PointerRNA *properties)
{
	const wmManipulatorType *wt = WM_manipulatortype_find(idname, false);
	return WM_manipulator_new_ptr(wt, mgroup, properties);
}

/**
 * Initialize default values and allocate needed memory for members.
 */
static void manipulator_init(wmManipulator *mpr)
{
	const float color_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	mpr->scale_basis = 1.0f;
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
static void wm_manipulator_register(wmManipulatorGroup *mgroup, wmManipulator *mpr)
{
	manipulator_init(mpr);
	wm_manipulatorgroup_manipulator_register(mgroup, mpr);
}

/**
 * \warning this doesn't check #wmManipulatorMap (highlight, selection etc).
 * Typical use is when freeing the windowing data,
 * where caller can manage clearing selection, highlight... etc.
 */
void WM_manipulator_free(wmManipulator *mpr)
{
	if (mpr->type->free != NULL) {
		mpr->type->free(mpr);
	}

#ifdef WITH_PYTHON
	if (mpr->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(mpr->py_instance);
	}
#endif

	if (mpr->op_data) {
		for (int i = 0; i < mpr->op_data_len; i++) {
			WM_operator_properties_free(&mpr->op_data[i].ptr);
		}
		MEM_freeN(mpr->op_data);
	}

	if (mpr->ptr != NULL) {
		WM_manipulator_properties_free(mpr->ptr);
		MEM_freeN(mpr->ptr);
	}

	if (mpr->type->target_property_defs_len != 0) {
		wmManipulatorProperty *mpr_prop_array = WM_manipulator_target_property_array(mpr);
		for (int i = 0; i < mpr->type->target_property_defs_len; i++) {
			wmManipulatorProperty *mpr_prop = &mpr_prop_array[i];
			if (mpr_prop->custom_func.free_fn) {
				mpr_prop->custom_func.free_fn(mpr, mpr_prop);
			}
		}
	}

	MEM_freeN(mpr);
}

/**
 * Free \a manipulator and unlink from \a manipulatorlist.
 * \a manipulatorlist is allowed to be NULL.
 */
void WM_manipulator_unlink(ListBase *manipulatorlist, wmManipulatorMap *mmap, wmManipulator *mpr, bContext *C)
{
	if (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) {
		wm_manipulatormap_highlight_set(mmap, C, NULL, 0);
	}
	if (mpr->state & WM_MANIPULATOR_STATE_MODAL) {
		wm_manipulatormap_modal_set(mmap, C, mpr, NULL, false);
	}
	/* Unlink instead of setting so we don't run callbacks. */
	if (mpr->state & WM_MANIPULATOR_STATE_SELECT) {
		WM_manipulator_select_unlink(mmap, mpr);
	}

	if (manipulatorlist) {
		BLI_remlink(manipulatorlist, mpr);
	}

	BLI_assert(mmap->mmap_context.highlight != mpr);
	BLI_assert(mmap->mmap_context.modal != mpr);

	WM_manipulator_free(mpr);
}

/* -------------------------------------------------------------------- */
/** \name Manipulator Creation API
 *
 * API for defining data on manipulator creation.
 *
 * \{ */

struct wmManipulatorOpElem *WM_manipulator_operator_get(
        wmManipulator *mpr, int part_index)
{
	if (mpr->op_data && ((part_index >= 0) && (part_index < mpr->op_data_len))) {
		return &mpr->op_data[part_index];
	}
	return NULL;
}

PointerRNA *WM_manipulator_operator_set(
        wmManipulator *mpr, int part_index,
        wmOperatorType *ot, IDProperty *properties)
{
	BLI_assert(part_index < 255);
	/* We could pre-allocate these but using multiple is such a rare thing. */
	if (part_index >= mpr->op_data_len) {
		mpr->op_data_len = part_index + 1;
		mpr->op_data = MEM_recallocN(mpr->op_data, sizeof(*mpr->op_data) * mpr->op_data_len);
	}
	wmManipulatorOpElem *mpop = &mpr->op_data[part_index];
	mpop->type = ot;

	if (mpop->ptr.data) {
		WM_operator_properties_free(&mpop->ptr);
	}
	WM_operator_properties_create_ptr(&mpop->ptr, ot);

	if (properties) {
		mpop->ptr.data = properties;
	}

	return &mpop->ptr;
}

static void wm_manipulator_set_matrix_rotation_from_z_axis__internal(
        float matrix[4][4], const float z_axis[3])
{
	/* old code, seems we can use simpler method */
#if 0
	const float z_global[3] = {0.0f, 0.0f, 1.0f};
	float rot[3][3];

	rotation_between_vecs_to_mat3(rot, z_global, z_axis);
	copy_v3_v3(matrix[0], rot[0]);
	copy_v3_v3(matrix[1], rot[1]);
	copy_v3_v3(matrix[2], rot[2]);
#else
	normalize_v3_v3(matrix[2], z_axis);
	ortho_basis_v3v3_v3(matrix[0], matrix[1], matrix[2]);
#endif

}

static void wm_manipulator_set_matrix_rotation_from_yz_axis__internal(
        float matrix[4][4], const float y_axis[3], const float z_axis[3])
{
	normalize_v3_v3(matrix[1], y_axis);
	normalize_v3_v3(matrix[2], z_axis);
	cross_v3_v3v3(matrix[0], matrix[1], matrix[2]);
	normalize_v3(matrix[0]);
}

/**
 * wmManipulator.matrix utils.
 */
void WM_manipulator_set_matrix_rotation_from_z_axis(
        wmManipulator *mpr, const float z_axis[3])
{
	wm_manipulator_set_matrix_rotation_from_z_axis__internal(mpr->matrix_basis, z_axis);
}
void WM_manipulator_set_matrix_rotation_from_yz_axis(
        wmManipulator *mpr, const float y_axis[3], const float z_axis[3])
{
	wm_manipulator_set_matrix_rotation_from_yz_axis__internal(mpr->matrix_basis, y_axis, z_axis);
}
void WM_manipulator_set_matrix_location(wmManipulator *mpr, const float origin[3])
{
	copy_v3_v3(mpr->matrix_basis[3], origin);
}

/**
 * wmManipulator.matrix_offset utils.
 */
void WM_manipulator_set_matrix_offset_rotation_from_z_axis(
        wmManipulator *mpr, const float z_axis[3])
{
	wm_manipulator_set_matrix_rotation_from_z_axis__internal(mpr->matrix_offset, z_axis);
}
void WM_manipulator_set_matrix_offset_rotation_from_yz_axis(
        wmManipulator *mpr, const float y_axis[3], const float z_axis[3])
{
	wm_manipulator_set_matrix_rotation_from_yz_axis__internal(mpr->matrix_offset, y_axis, z_axis);
}
void WM_manipulator_set_matrix_offset_location(wmManipulator *mpr, const float offset[3])
{
	copy_v3_v3(mpr->matrix_offset[3], offset);
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
	mpr->scale_basis = scale;
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
void WM_manipulator_get_color(const wmManipulator *mpr, float color[4])
{
	copy_v4_v4(color, mpr->color);
}
void WM_manipulator_set_color(wmManipulator *mpr, const float color[4])
{
	copy_v4_v4(mpr->color, color);
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
 * Add/Remove \a manipulator to selection.
 * Reallocates memory for selected manipulators so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_manipulator_select_set_ex(
        wmManipulatorMap *mmap, wmManipulator *mpr, bool select,
        bool use_array, bool use_callback)
{
	bool changed = false;

	if (select) {
		if ((mpr->state & WM_MANIPULATOR_STATE_SELECT) == 0) {
			if (use_array) {
				wm_manipulatormap_select_array_push_back(mmap, mpr);
			}
			mpr->state |= WM_MANIPULATOR_STATE_SELECT;
			changed = true;
		}
	}
	else {
		if (mpr->state & WM_MANIPULATOR_STATE_SELECT) {
			if (use_array) {
				wm_manipulatormap_select_array_remove(mmap, mpr);
			}
			mpr->state &= ~WM_MANIPULATOR_STATE_SELECT;
			changed = true;
		}
	}

	/* In the case of unlinking we only want to remove from the array
	 * and not write to the external state */
	if (use_callback && changed) {
		if (mpr->type->select_refresh) {
			mpr->type->select_refresh(mpr);
		}
	}

	return changed;
}

/* Remove from selection array without running callbacks. */
bool WM_manipulator_select_unlink(wmManipulatorMap *mmap, wmManipulator *mpr)
{
	return wm_manipulator_select_set_ex(mmap, mpr, false, true, false);
}

bool WM_manipulator_select_set(wmManipulatorMap *mmap, wmManipulator *mpr, bool select)
{
	return wm_manipulator_select_set_ex(mmap, mpr, select, true, true);
}

void WM_manipulator_highlight_set(wmManipulatorMap *mmap, wmManipulator *mpr)
{
	wm_manipulatormap_highlight_set(mmap, NULL, mpr, mpr ? mpr->highlight_part : 0);
}

bool wm_manipulator_select_and_highlight(bContext *C, wmManipulatorMap *mmap, wmManipulator *mpr)
{
	if (WM_manipulator_select_set(mmap, mpr, true)) {
		wm_manipulatormap_highlight_set(mmap, C, mpr, mpr->highlight_part);
		return true;
	}
	else {
		return false;
	}
}

/**
 * Special function to run from setup so manipulators start out interactive.
 *
 * We could do this when linking them, but this complicates things since the window update code needs to run first.
 */
void WM_manipulator_modal_set_from_setup(
        struct wmManipulatorMap *mmap, struct bContext *C,
        struct wmManipulator *mpr, int part_index, const wmEvent *event)
{
	mpr->highlight_part = part_index;
	WM_manipulator_highlight_set(mmap, mpr);
	if (false) {
		wm_manipulatormap_modal_set(mmap, C, mpr, event, true);
	}
	else {
		/* WEAK: but it works. */
		WM_operator_name_call(C, "MANIPULATORGROUP_OT_manipulator_tweak", WM_OP_INVOKE_DEFAULT, NULL);
	}
}

void wm_manipulator_calculate_scale(wmManipulator *mpr, const bContext *C)
{
	const RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float scale = UI_DPI_FAC;

	if ((mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SCALE) == 0) {
		scale *= U.manipulator_size;
		if (rv3d) {
			/* 'ED_view3d_pixel_size' includes 'U.pixelsize', remove it. */
			float matrix_world[4][4];
			if (mpr->type->matrix_basis_get) {
				float matrix_basis[4][4];
				mpr->type->matrix_basis_get(mpr, matrix_basis);
				mul_m4_m4m4(matrix_world, mpr->matrix_space, matrix_basis);
			}
			else {
				mul_m4_m4m4(matrix_world, mpr->matrix_space, mpr->matrix_basis);
			}

			/* Exclude matrix_offset from scale. */
			scale *= ED_view3d_pixel_size(rv3d, matrix_world[3]) / U.pixelsize;
		}
		else {
			scale *= 0.02f;
		}
	}

	mpr->scale_final = mpr->scale_basis * scale;
}

static void manipulator_update_prop_data(wmManipulator *mpr)
{
	/* manipulator property might have been changed, so update manipulator */
	if (mpr->type->property_update) {
		wmManipulatorProperty *mpr_prop_array = WM_manipulator_target_property_array(mpr);
		for (int i = 0; i < mpr->type->target_property_defs_len; i++) {
			wmManipulatorProperty *mpr_prop = &mpr_prop_array[i];
			if (WM_manipulator_target_property_is_valid(mpr_prop)) {
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

int wm_manipulator_is_visible(wmManipulator *mpr)
{
	if (mpr->flag & WM_MANIPULATOR_HIDDEN) {
		return 0;
	}
	if ((mpr->state & WM_MANIPULATOR_STATE_MODAL) &&
	    !(mpr->flag & (WM_MANIPULATOR_DRAW_MODAL | WM_MANIPULATOR_DRAW_VALUE)))
	{
		/* don't draw while modal (dragging) */
		return 0;
	}
	if ((mpr->flag & WM_MANIPULATOR_DRAW_HOVER) &&
	    !(mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) &&
	    !(mpr->state & WM_MANIPULATOR_STATE_SELECT)) /* still draw selected manipulators */
	{
		/* update but don't draw */
		return WM_MANIPULATOR_IS_VISIBLE_UPDATE;
	}

	return WM_MANIPULATOR_IS_VISIBLE_UPDATE | WM_MANIPULATOR_IS_VISIBLE_DRAW;
}

void WM_manipulator_calc_matrix_final_params(
        const wmManipulator *mpr,
        const struct WM_ManipulatorMatrixParams *params,
        float r_mat[4][4])
{
	const float (* const matrix_space)[4]  = params->matrix_space  ? params->matrix_space  : mpr->matrix_space;
	const float (* const matrix_basis)[4]  = params->matrix_basis  ? params->matrix_basis  : mpr->matrix_basis;
	const float (* const matrix_offset)[4] = params->matrix_offset ? params->matrix_offset : mpr->matrix_offset;
	const float *scale_final = params->scale_final ? params->scale_final : &mpr->scale_final;

	float final_matrix[4][4];
	if (params->matrix_basis == NULL && mpr->type->matrix_basis_get) {
		mpr->type->matrix_basis_get(mpr, final_matrix);
	}
	else {
		copy_m4_m4(final_matrix, matrix_basis);
	}

	if (mpr->flag & WM_MANIPULATOR_DRAW_NO_SCALE) {
		mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
	}
	else {
		if (mpr->flag & WM_MANIPULATOR_DRAW_OFFSET_SCALE) {
			mul_mat3_m4_fl(final_matrix, *scale_final);
			mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
		}
		else {
			mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
			mul_mat3_m4_fl(final_matrix, *scale_final);
		}
	}

	mul_m4_m4m4(r_mat, matrix_space, final_matrix);
}

void WM_manipulator_calc_matrix_final(const wmManipulator *mpr, float r_mat[4][4])
{
	WM_manipulator_calc_matrix_final_params(
	        mpr,
	        &((struct WM_ManipulatorMatrixParams) {
	            .matrix_space = NULL,
	            .matrix_basis = NULL,
	            .matrix_offset = NULL,
	            .scale_final = NULL,
	        }), r_mat
	);
}

/** \name Manipulator Propery Access
 *
 * Matches `WM_operator_properties` conventions.
 *
 * \{ */


void WM_manipulator_properties_create_ptr(PointerRNA *ptr, wmManipulatorType *wt)
{
	RNA_pointer_create(NULL, wt->srna, NULL, ptr);
}

void WM_manipulator_properties_create(PointerRNA *ptr, const char *wtstring)
{
	const wmManipulatorType *wt = WM_manipulatortype_find(wtstring, false);

	if (wt)
		WM_manipulator_properties_create_ptr(ptr, (wmManipulatorType *)wt);
	else
		RNA_pointer_create(NULL, &RNA_ManipulatorProperties, NULL, ptr);
}

/* similar to the function above except its uses ID properties
 * used for keymaps and macros */
void WM_manipulator_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *wtstring)
{
	if (*properties == NULL) {
		IDPropertyTemplate val = {0};
		*properties = IDP_New(IDP_GROUP, &val, "wmOpItemProp");
	}

	if (*ptr == NULL) {
		*ptr = MEM_callocN(sizeof(PointerRNA), "wmOpItemPtr");
		WM_manipulator_properties_create(*ptr, wtstring);
	}

	(*ptr)->data = *properties;

}

void WM_manipulator_properties_sanitize(PointerRNA *ptr, const bool no_context)
{
	RNA_STRUCT_BEGIN (ptr, prop)
	{
		switch (RNA_property_type(prop)) {
			case PROP_ENUM:
				if (no_context)
					RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
				else
					RNA_def_property_clear_flag(prop, PROP_ENUM_NO_CONTEXT);
				break;
			case PROP_POINTER:
			{
				StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

				/* recurse into manipulator properties */
				if (RNA_struct_is_a(ptype, &RNA_ManipulatorProperties)) {
					PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
					WM_manipulator_properties_sanitize(&opptr, no_context);
				}
				break;
			}
			default:
				break;
		}
	}
	RNA_STRUCT_END;
}


/** set all props to their default,
 * \param do_update Only update un-initialized props.
 *
 * \note, theres nothing specific to manipulators here.
 * this could be made a general function.
 */
bool WM_manipulator_properties_default(PointerRNA *ptr, const bool do_update)
{
	bool changed = false;
	RNA_STRUCT_BEGIN (ptr, prop)
	{
		switch (RNA_property_type(prop)) {
			case PROP_POINTER:
			{
				StructRNA *ptype = RNA_property_pointer_type(ptr, prop);
				if (ptype != &RNA_Struct) {
					PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
					changed |= WM_manipulator_properties_default(&opptr, do_update);
				}
				break;
			}
			default:
				if ((do_update == false) || (RNA_property_is_set(ptr, prop) == false)) {
					if (RNA_property_reset(ptr, prop, -1)) {
						changed = true;
					}
				}
				break;
		}
	}
	RNA_STRUCT_END;

	return changed;
}

/* remove all props without PROP_SKIP_SAVE */
void WM_manipulator_properties_reset(wmManipulator *mpr)
{
	if (mpr->ptr->data) {
		PropertyRNA *iterprop;
		iterprop = RNA_struct_iterator_property(mpr->type->srna);

		RNA_PROP_BEGIN (mpr->ptr, itemptr, iterprop)
		{
			PropertyRNA *prop = itemptr.data;

			if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
				const char *identifier = RNA_property_identifier(prop);
				RNA_struct_idprops_unset(mpr->ptr, identifier);
			}
		}
		RNA_PROP_END;
	}
}

void WM_manipulator_properties_clear(PointerRNA *ptr)
{
	IDProperty *properties = ptr->data;

	if (properties) {
		IDP_ClearProperty(properties);
	}
}

void WM_manipulator_properties_free(PointerRNA *ptr)
{
	IDProperty *properties = ptr->data;

	if (properties) {
		IDP_FreeProperty(properties);
		MEM_freeN(properties);
		ptr->data = NULL; /* just in case */
	}
}

/** \} */

/** \name General Utilities
 *
 * \{ */

bool WM_manipulator_context_check_drawstep(const struct bContext *C, eWM_ManipulatorMapDrawStep step)
{
	switch (step) {
		case WM_MANIPULATORMAP_DRAWSTEP_2D:
		{
			break;
		}
		case WM_MANIPULATORMAP_DRAWSTEP_3D:
		{
			wmWindowManager *wm = CTX_wm_manager(C);
			if (ED_screen_animation_playing(wm)) {
				return false;
			}
			break;
		}
	}
	return true;
}

/** \} */
