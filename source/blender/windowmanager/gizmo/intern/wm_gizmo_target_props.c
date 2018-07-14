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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/gizmo/intern/wm_gizmo_target_props.c
 *  \ingroup wm
 */

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "wm.h"

#include "ED_screen.h"
#include "ED_view3d.h"

/* own includes */
#include "wm_gizmo_wmapi.h"
#include "wm_gizmo_intern.h"

/* -------------------------------------------------------------------- */

/** \name Property Definition
 * \{ */

BLI_INLINE wmGizmoProperty *wm_gizmo_target_property_array(wmGizmo *mpr)
{
	return (wmGizmoProperty *)(POINTER_OFFSET(mpr, mpr->type->struct_size));
}

wmGizmoProperty *WM_gizmo_target_property_array(wmGizmo *mpr)
{
	return wm_gizmo_target_property_array(mpr);
}

wmGizmoProperty *WM_gizmo_target_property_at_index(wmGizmo *mpr, int index)
{
	BLI_assert(index < mpr->type->target_property_defs_len);
	BLI_assert(index != -1);
	wmGizmoProperty *mpr_prop_array = wm_gizmo_target_property_array(mpr);
	return &mpr_prop_array[index];
}

wmGizmoProperty *WM_gizmo_target_property_find(wmGizmo *mpr, const char *idname)
{
	int index = BLI_findstringindex(
	        &mpr->type->target_property_defs, idname, offsetof(wmGizmoPropertyType, idname));
	if (index != -1) {
		return WM_gizmo_target_property_at_index(mpr, index);
	}
	else {
		return NULL;
	}
}

void WM_gizmo_target_property_def_rna_ptr(
        wmGizmo *mpr, const wmGizmoPropertyType *mpr_prop_type,
        PointerRNA *ptr, PropertyRNA *prop, int index)
{
	wmGizmoProperty *mpr_prop = WM_gizmo_target_property_at_index(mpr, mpr_prop_type->index_in_type);

	/* if gizmo evokes an operator we cannot use it for property manipulation */
	BLI_assert(mpr->op_data == NULL);

	mpr_prop->type = mpr_prop_type;

	mpr_prop->ptr = *ptr;
	mpr_prop->prop = prop;
	mpr_prop->index = index;

	if (mpr->type->property_update) {
		mpr->type->property_update(mpr, mpr_prop);
	}
}

void WM_gizmo_target_property_def_rna(
        wmGizmo *mpr, const char *idname,
        PointerRNA *ptr, const char *propname, int index)
{
	const wmGizmoPropertyType *mpr_prop_type = WM_gizmotype_target_property_find(mpr->type, idname);
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	WM_gizmo_target_property_def_rna_ptr(mpr, mpr_prop_type, ptr, prop, index);
}

void WM_gizmo_target_property_def_func_ptr(
        wmGizmo *mpr, const wmGizmoPropertyType *mpr_prop_type,
        const wmGizmoPropertyFnParams *params)
{
	wmGizmoProperty *mpr_prop = WM_gizmo_target_property_at_index(mpr, mpr_prop_type->index_in_type);

	/* if gizmo evokes an operator we cannot use it for property manipulation */
	BLI_assert(mpr->op_data == NULL);

	mpr_prop->type = mpr_prop_type;

	mpr_prop->custom_func.value_get_fn = params->value_get_fn;
	mpr_prop->custom_func.value_set_fn = params->value_set_fn;
	mpr_prop->custom_func.range_get_fn = params->range_get_fn;
	mpr_prop->custom_func.free_fn = params->free_fn;
	mpr_prop->custom_func.user_data = params->user_data;

	if (mpr->type->property_update) {
		mpr->type->property_update(mpr, mpr_prop);
	}
}

void WM_gizmo_target_property_def_func(
        wmGizmo *mpr, const char *idname,
        const wmGizmoPropertyFnParams *params)
{
	const wmGizmoPropertyType *mpr_prop_type = WM_gizmotype_target_property_find(mpr->type, idname);
	WM_gizmo_target_property_def_func_ptr(mpr, mpr_prop_type, params);
}

void WM_gizmo_target_property_clear_rna_ptr(
        wmGizmo *mpr, const wmGizmoPropertyType *mpr_prop_type)
{
	wmGizmoProperty *mpr_prop = WM_gizmo_target_property_at_index(mpr, mpr_prop_type->index_in_type);

	/* if gizmo evokes an operator we cannot use it for property manipulation */
	BLI_assert(mpr->op_data == NULL);

	mpr_prop->type = NULL;

	mpr_prop->ptr = PointerRNA_NULL;
	mpr_prop->prop = NULL;
	mpr_prop->index = -1;
}

void WM_gizmo_target_property_clear_rna(
        wmGizmo *mpr, const char *idname)
{
	const wmGizmoPropertyType *mpr_prop_type = WM_gizmotype_target_property_find(mpr->type, idname);
	WM_gizmo_target_property_clear_rna_ptr(mpr, mpr_prop_type);
}


/** \} */


/* -------------------------------------------------------------------- */

/** \name Property Access
 * \{ */

bool WM_gizmo_target_property_is_valid_any(wmGizmo *mpr)
{
	wmGizmoProperty *mpr_prop_array = wm_gizmo_target_property_array(mpr);
	for (int i = 0; i < mpr->type->target_property_defs_len; i++) {
		wmGizmoProperty *mpr_prop = &mpr_prop_array[i];
		if (WM_gizmo_target_property_is_valid(mpr_prop)) {
			return true;
		}
	}
	return false;
}

bool WM_gizmo_target_property_is_valid(const wmGizmoProperty *mpr_prop)
{
	return  ((mpr_prop->prop != NULL) ||
	         (mpr_prop->custom_func.value_get_fn && mpr_prop->custom_func.value_set_fn));
}

float WM_gizmo_target_property_value_get(
        const wmGizmo *mpr, wmGizmoProperty *mpr_prop)
{
	if (mpr_prop->custom_func.value_get_fn) {
		float value = 0.0f;
		BLI_assert(mpr_prop->type->array_length == 1);
		mpr_prop->custom_func.value_get_fn(mpr, mpr_prop, &value);
		return value;
	}

	if (mpr_prop->index == -1) {
		return RNA_property_float_get(&mpr_prop->ptr, mpr_prop->prop);
	}
	else {
		return RNA_property_float_get_index(&mpr_prop->ptr, mpr_prop->prop, mpr_prop->index);
	}
}

void WM_gizmo_target_property_value_set(
        bContext *C, const wmGizmo *mpr,
        wmGizmoProperty *mpr_prop, const float value)
{
	if (mpr_prop->custom_func.value_set_fn) {
		BLI_assert(mpr_prop->type->array_length == 1);
		mpr_prop->custom_func.value_set_fn(mpr, mpr_prop, &value);
		return;
	}

	/* reset property */
	if (mpr_prop->index == -1) {
		RNA_property_float_set(&mpr_prop->ptr, mpr_prop->prop, value);
	}
	else {
		RNA_property_float_set_index(&mpr_prop->ptr, mpr_prop->prop, mpr_prop->index, value);
	}
	RNA_property_update(C, &mpr_prop->ptr, mpr_prop->prop);
}

void WM_gizmo_target_property_value_get_array(
        const wmGizmo *mpr, wmGizmoProperty *mpr_prop,
        float *value)
{
	if (mpr_prop->custom_func.value_get_fn) {
		mpr_prop->custom_func.value_get_fn(mpr, mpr_prop, value);
		return;
	}
	RNA_property_float_get_array(&mpr_prop->ptr, mpr_prop->prop, value);
}

void WM_gizmo_target_property_value_set_array(
        bContext *C, const wmGizmo *mpr, wmGizmoProperty *mpr_prop,
        const float *value)
{
	if (mpr_prop->custom_func.value_set_fn) {
		mpr_prop->custom_func.value_set_fn(mpr, mpr_prop, value);
		return;
	}
	RNA_property_float_set_array(&mpr_prop->ptr, mpr_prop->prop, value);

	RNA_property_update(C, &mpr_prop->ptr, mpr_prop->prop);
}

bool WM_gizmo_target_property_range_get(
        const wmGizmo *mpr, wmGizmoProperty *mpr_prop,
        float range[2])
{
	if (mpr_prop->custom_func.value_get_fn) {
		if (mpr_prop->custom_func.range_get_fn) {
			mpr_prop->custom_func.range_get_fn(mpr, mpr_prop, range);
			return true;
		}
		else {
			return false;

		}
	}

	float step, precision;
	RNA_property_float_ui_range(&mpr_prop->ptr, mpr_prop->prop, &range[0], &range[1], &step, &precision);
	return true;
}

int WM_gizmo_target_property_array_length(
        const wmGizmo *UNUSED(mpr), wmGizmoProperty *mpr_prop)
{
	if (mpr_prop->custom_func.value_get_fn) {
		return mpr_prop->type->array_length;
	}
	return RNA_property_array_length(&mpr_prop->ptr, mpr_prop->prop);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Property Define
 * \{ */

const wmGizmoPropertyType *WM_gizmotype_target_property_find(
        const wmGizmoType *wt, const char *idname)
{
	return BLI_findstring(&wt->target_property_defs, idname, offsetof(wmGizmoPropertyType, idname));
}

void WM_gizmotype_target_property_def(
        wmGizmoType *wt, const char *idname, int data_type, int array_length)
{
	wmGizmoPropertyType *mpt;

	BLI_assert(WM_gizmotype_target_property_find(wt, idname) == NULL);

	const uint idname_size = strlen(idname) + 1;
	mpt = MEM_callocN(sizeof(wmGizmoPropertyType) + idname_size, __func__);
	memcpy(mpt->idname, idname, idname_size);
	mpt->data_type = data_type;
	mpt->array_length = array_length;
	mpt->index_in_type = wt->target_property_defs_len;
	wt->target_property_defs_len += 1;
	BLI_addtail(&wt->target_property_defs, mpt);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Property Utilities
 * \{ */

void WM_gizmo_do_msg_notify_tag_refresh(
        bContext *UNUSED(C), wmMsgSubscribeKey *UNUSED(msg_key), wmMsgSubscribeValue *msg_val)
{
	ARegion *ar = msg_val->owner;
	wmGizmoMap *mmap = msg_val->user_data;

	ED_region_tag_redraw(ar);
	WM_gizmomap_tag_refresh(mmap);
}

/**
 * Runs on the "prepare draw" pass,
 * drawing the region clears.
 */
void WM_gizmo_target_property_subscribe_all(
        wmGizmo *mpr, struct wmMsgBus *mbus, ARegion *ar)
{
	if (mpr->type->target_property_defs_len) {
		wmGizmoProperty *mpr_prop_array = WM_gizmo_target_property_array(mpr);
		for (int i = 0; i < mpr->type->target_property_defs_len; i++) {
			wmGizmoProperty *mpr_prop = &mpr_prop_array[i];
			if (WM_gizmo_target_property_is_valid(mpr_prop)) {
				if (mpr_prop->prop) {
					WM_msg_subscribe_rna(
					        mbus, &mpr_prop->ptr, mpr_prop->prop,
					        &(const wmMsgSubscribeValue){
					            .owner = ar,
					            .user_data = ar,
					            .notify = ED_region_do_msg_notify_tag_redraw,
					        }, __func__);
					WM_msg_subscribe_rna(
					        mbus, &mpr_prop->ptr, mpr_prop->prop,
					        &(const wmMsgSubscribeValue){
					            .owner = ar,
					            .user_data = mpr->parent_mgroup->parent_mmap,
					            .notify = WM_gizmo_do_msg_notify_tag_refresh,
					        }, __func__);
				}
			}
		}
	}
}

/** \} */
