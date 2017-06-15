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

/** \file blender/windowmanager/manipulators/intern/wm_manipulator_property.c
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

/* factor for precision tweaking */
#define MANIPULATOR_PRECISION_FAC 0.05f

/* -------------------------------------------------------------------- */

/** \name Property Definition
 * \{ */

wmManipulatorProperty *WM_manipulator_property_find(wmManipulator *mpr, const char *idname)
{
	return BLI_findstring(&mpr->properties, idname, offsetof(wmManipulatorProperty, idname));
}

static wmManipulatorProperty *wm_manipulator_property_def_internal(
        wmManipulator *mpr, const char *idname)
{
	wmManipulatorProperty *mpr_prop = WM_manipulator_property_find(mpr, idname);

	if (mpr_prop == NULL) {
		const uint idname_size = strlen(idname) + 1;
		mpr_prop = MEM_callocN(sizeof(wmManipulatorProperty) + idname_size, __func__);
		memcpy(mpr_prop->idname, idname, idname_size);
		BLI_addtail(&mpr->properties, mpr_prop);
	}
	return mpr_prop;
}

void WM_manipulator_property_def_rna(
        wmManipulator *mpr, const char *idname,
        PointerRNA *ptr, const char *propname, int index)
{
	wmManipulatorProperty *mpr_prop = wm_manipulator_property_def_internal(mpr, idname);

	/* if manipulator evokes an operator we cannot use it for property manipulation */
	mpr->opname = NULL;

	mpr_prop->ptr = *ptr;
	mpr_prop->prop = RNA_struct_find_property(ptr, propname);
	mpr_prop->index = index;

	if (mpr->type->property_update) {
		mpr->type->property_update(mpr, mpr_prop);
	}
}

void WM_manipulator_property_def_func(
        wmManipulator *mpr, const char *idname,
        const wmManipulatorPropertyFnParams *params)
{
	wmManipulatorProperty *mpr_prop = wm_manipulator_property_def_internal(mpr, idname);

	/* if manipulator evokes an operator we cannot use it for property manipulation */
	mpr->opname = NULL;

	mpr_prop->custom_func.value_get_fn = params->value_get_fn;
	mpr_prop->custom_func.value_set_fn = params->value_set_fn;
	mpr_prop->custom_func.range_get_fn = params->range_get_fn;
	mpr_prop->custom_func.user_data = params->user_data;

	if (mpr->type->property_update) {
		mpr->type->property_update(mpr, mpr_prop);
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Property Access
 * \{ */

bool WM_manipulator_property_is_valid(const wmManipulatorProperty *mpr_prop)
{
	return  ((mpr_prop->prop != NULL) ||
	         (mpr_prop->custom_func.value_get_fn && mpr_prop->custom_func.value_set_fn));
}

void WM_manipulator_property_value_set(
        bContext *C, const wmManipulator *mpr,
        wmManipulatorProperty *mpr_prop, const float value)
{
	if (mpr_prop->custom_func.value_set_fn) {
		mpr_prop->custom_func.value_set_fn(mpr, mpr_prop, mpr_prop->custom_func.user_data, &value, 1);
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

float WM_manipulator_property_value_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
{
	if (mpr_prop->custom_func.value_get_fn) {
		float value = 0.0f;
		mpr_prop->custom_func.value_get_fn(mpr, mpr_prop, mpr_prop->custom_func.user_data, &value, 1);
		return value;
	}

	if (mpr_prop->index == -1) {
		return RNA_property_float_get(&mpr_prop->ptr, mpr_prop->prop);
	}
	else {
		return RNA_property_float_get_index(&mpr_prop->ptr, mpr_prop->prop, mpr_prop->index);
	}
}

void WM_manipulator_property_range_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        float range[2])
{
	if (mpr_prop->custom_func.range_get_fn) {
		mpr_prop->custom_func.range_get_fn(mpr, mpr_prop, mpr_prop->custom_func.user_data, range);
		return;
	}

	float step, precision;
	RNA_property_float_ui_range(&mpr_prop->ptr, mpr_prop->prop, &range[0], &range[1], &step, &precision);
}

/** \} */
