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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file manipulator_library_utils.c
 *  \ingroup wm
 *
 * \name Manipulator Library Utilities
 *
 * \brief This file contains functions for common behaviors of manipulators.
 */

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_view3d.h"

/* own includes */
#include "manipulator_library_intern.h"

/* factor for precision tweaking */
#define MANIPULATOR_PRECISION_FAC 0.05f


BLI_INLINE float manipulator_offset_from_value_constr(
        const float range_fac, const float min, const float range, const float value,
        const bool inverted)
{
	return inverted ? (range_fac * (min + range - value) / range) : (range_fac * (value / range));
}

BLI_INLINE float manipulator_value_from_offset_constr(
        const float range_fac, const float min, const float range, const float value,
        const bool inverted)
{
	return inverted ? (min + range - (value * range / range_fac)) : (value * range / range_fac);
}

float manipulator_offset_from_value(
        ManipulatorCommonData *data, const float value, const bool constrained, const bool inverted)
{
	if (constrained)
		return manipulator_offset_from_value_constr(data->range_fac, data->min, data->range, value, inverted);

	return value;
}

float manipulator_value_from_offset(
        ManipulatorCommonData *data, ManipulatorInteraction *inter, const float offset,
        const bool constrained, const bool inverted, const bool use_precision)
{
	const float max = data->min + data->range;

	if (use_precision) {
		/* add delta offset of this step to total precision_offset */
		inter->precision_offset += offset - inter->prev_offset;
	}
	inter->prev_offset = offset;

	float ofs_new = inter->init_offset + offset - inter->precision_offset * (1.0f - MANIPULATOR_PRECISION_FAC);
	float value;

	if (constrained) {
		value = manipulator_value_from_offset_constr(data->range_fac, data->min, data->range, ofs_new, inverted);
	}
	else {
		value = ofs_new;
	}

	/* clamp to custom range */
	if (data->flag & MANIPULATOR_CUSTOM_RANGE_SET) {
		CLAMP(value, data->min, max);
	}

	return value;
}

void manipulator_property_data_update(
        wmManipulator *mpr, ManipulatorCommonData *data, wmManipulatorProperty *mpr_prop,
        const bool constrained, const bool inverted)
{
	if (mpr_prop->custom_func.value_get_fn != NULL) {
		/* pass  */
	}
	else if (mpr_prop->prop != NULL) {
		/* pass  */
	}
	else {
		data->offset = 0.0f;
		return;
	}

	float value = WM_manipulator_target_property_value_get(mpr, mpr_prop);

	if (constrained) {
		if ((data->flag & MANIPULATOR_CUSTOM_RANGE_SET) == 0) {
			float range[2];
			if (WM_manipulator_target_property_range_get(mpr, mpr_prop, range)) {
				data->range = range[1] - range[0];
				data->min = range[0];
			}
			else {
				BLI_assert(0);
			}
		}
		data->offset = manipulator_offset_from_value_constr(data->range_fac, data->min, data->range, value, inverted);
	}
	else {
		data->offset = value;
	}
}

void manipulator_property_value_reset(
        bContext *C, const wmManipulator *mpr, ManipulatorInteraction *inter,
        wmManipulatorProperty *mpr_prop)
{
	WM_manipulator_target_property_value_set(C, mpr, mpr_prop, inter->init_value);
}

/* -------------------------------------------------------------------- */

void manipulator_color_get(
        const wmManipulator *mpr, const bool highlight,
        float r_col[4])
{
	if (highlight && !(mpr->flag & WM_MANIPULATOR_DRAW_HOVER)) {
		copy_v4_v4(r_col, mpr->color_hi);
	}
	else {
		copy_v4_v4(r_col, mpr->color);
	}
}

/* -------------------------------------------------------------------- */

/**
 * Takes mouse coordinates and returns them in relation to the manipulator.
 * Both 2D & 3D supported, use so we can use 2D manipulators in the 3D view.
 */
bool manipulator_window_project_2d(
        bContext *C, const struct wmManipulator *mpr, const float mval[2], int axis, bool use_offset,
        float r_co[2])
{
	float mat[4][4];
	{
		float mat_identity[4][4];
		struct WM_ManipulatorMatrixParams params = {NULL};
		if (use_offset == false) {
			unit_m4(mat_identity);
			params.matrix_offset = mat_identity;
		}
		WM_manipulator_calc_matrix_final_params(mpr, &params, mat);
	}

	/* rotate mouse in relation to the center and relocate it */
	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		/* For 3d views, transform 2D mouse pos onto plane. */
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);

		float plane[4];

		plane_from_point_normal_v3(plane, mat[3], mat[2]);

		float ray_origin[3], ray_direction[3];

		if (ED_view3d_win_to_ray(CTX_data_depsgraph(C), ar, v3d, mval, ray_origin, ray_direction, false)) {
			float lambda;
			if (isect_ray_plane_v3(ray_origin, ray_direction, plane, &lambda, true)) {
				float co[3];
				madd_v3_v3v3fl(co, ray_origin, ray_direction, lambda);
				float imat[4][4];
				invert_m4_m4(imat, mat);
				mul_m4_v3(imat, co);
				r_co[0] = co[(axis + 1) % 3];
				r_co[1] = co[(axis + 2) % 3];
				return true;
			}
		}
		return false;
	}
	else {
		float co[3] = {mval[0], mval[1], 0.0f};
		float imat[4][4];
		invert_m4_m4(imat, mat);
		mul_m4_v3(imat, co);
		copy_v2_v2(r_co, co);
		return true;
	}
}
