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

/** \file blender/editors/space_view3d/view3d_manipulator_armature.c
 *  \ingroup spview3d
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_object_types.h"
#include "DNA_armature_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */


/* -------------------------------------------------------------------- */

/** \name Armature Spline Manipulator
 *
 * \{ */

/*
 * TODO(campbell): Current conversion is a approximation (usable not correct),
 * we'll need to take the next/previous bones into account to get the tangent directions.
 * First last matrices from 'b_bone_spline_setup' are close but also not quite accurate
 * since they're not at either end-points on the curve.
 *
 * Likely we'll need a function especially to get the first/last orientations.
 */

#define BBONE_SCALE_Y 3.0f

struct BoneSplineHandle {
	wmManipulator *manipulator;
	bPoseChannel *pchan;
	/* We could remove, keep since at the moment for checking the conversion. */
	float co[3];
	int index;
};

struct BoneSplineWidgetGroup {
	struct BoneSplineHandle handles[2];
};

static void manipulator_bbone_offset_get(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	struct BoneSplineHandle *bh = mpr_prop->custom_func.user_data;
	bPoseChannel *pchan = bh->pchan;

	float *value = value_p;
	BLI_assert(mpr_prop->type->array_length == 3);

	if (bh->index == 0) {
		bh->co[1] = pchan->bone->ease1 / BBONE_SCALE_Y;
		bh->co[0] = pchan->curveInX;
		bh->co[2] = pchan->curveInY;
	}
	else {
		bh->co[1] = -pchan->bone->ease2 / BBONE_SCALE_Y;
		bh->co[0] = pchan->curveOutX;
		bh->co[2] = pchan->curveOutY;
	}
	copy_v3_v3(value, bh->co);
}

static void manipulator_bbone_offset_set(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	struct BoneSplineHandle *bh = mpr_prop->custom_func.user_data;
	bPoseChannel *pchan = bh->pchan;

	const float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 3);
	copy_v3_v3(bh->co, value);

	if (bh->index == 0) {
		pchan->bone->ease1 = max_ff(0.0f, bh->co[1] * BBONE_SCALE_Y);
		pchan->curveInX = bh->co[0];
		pchan->curveInY = bh->co[2];
	}
	else {
		pchan->bone->ease2 = max_ff(0.0f, -bh->co[1] * BBONE_SCALE_Y);
		pchan->curveOutX = bh->co[0];
		pchan->curveOutY = bh->co[2];
	}

}

static bool WIDGETGROUP_armature_spline_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	if (ob != NULL) {
		const bArmature *arm = ob->data;
		if (arm->drawtype == ARM_B_BONE) {
			if (arm->act_bone && arm->act_bone->segments > 1) {
				View3D *v3d = CTX_wm_view3d(C);
				if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}


static void WIDGETGROUP_armature_spline_setup(const bContext *C, wmManipulatorGroup *mgroup)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bPoseChannel *pchan = BKE_pose_channel_active(ob);

	const wmManipulatorType *wt_grab = WM_manipulatortype_find("MANIPULATOR_WT_grab_3d", true);

	struct BoneSplineWidgetGroup *bspline_group = MEM_callocN(sizeof(struct BoneSplineWidgetGroup), __func__);
	mgroup->customdata = bspline_group;

	/* Handles */
	for (int i = 0; i < ARRAY_SIZE(bspline_group->handles); i++) {
		wmManipulator *mpr;
		mpr = bspline_group->handles[i].manipulator = WM_manipulator_new_ptr(wt_grab, mgroup, NULL);
		RNA_enum_set(mpr->ptr, "draw_style", ED_MANIPULATOR_GRAB_STYLE_RING_2D);
		RNA_enum_set(mpr->ptr, "draw_options",
		             ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL | ED_MANIPULATOR_GRAB_DRAW_FLAG_ALIGN_VIEW);
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_VALUE, true);

		UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
		UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);

		mpr->scale_basis = 0.06f;

		if (i == 0) {
			copy_v3_v3(mpr->matrix_basis[3], pchan->loc);
		}
	}
}

static void WIDGETGROUP_armature_spline_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));

	if (!mgroup->customdata)
		return;

	struct BoneSplineWidgetGroup *bspline_group = mgroup->customdata;
	bPoseChannel *pchan = BKE_pose_channel_active(ob);

	/* Handles */
	for (int i = 0; i < ARRAY_SIZE(bspline_group->handles); i++) {
		wmManipulator *mpr = bspline_group->handles[i].manipulator;
		bspline_group->handles[i].pchan = pchan;
		bspline_group->handles[i].index = i;

		float mat[4][4];
		mul_m4_m4m4(mat, ob->obmat, (i == 0) ? pchan->disp_mat : pchan->disp_tail_mat);
		copy_m4_m4(mpr->matrix_space, mat);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		WM_manipulator_target_property_def_func(
		        mpr, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_bbone_offset_get,
		            .value_set_fn = manipulator_bbone_offset_set,
		            .range_get_fn = NULL,
		            .user_data = &bspline_group->handles[i],
		        });
	}
}

void VIEW3D_WGT_armature_spline(wmManipulatorGroupType *wgt)
{
	wgt->name = "Armature Spline Widgets";
	wgt->idname = "VIEW3D_WGT_armature_spline";

	wgt->flag = (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	             WM_MANIPULATORGROUPTYPE_3D);

	wgt->poll = WIDGETGROUP_armature_spline_poll;
	wgt->setup = WIDGETGROUP_armature_spline_setup;
	wgt->refresh = WIDGETGROUP_armature_spline_refresh;
}

/** \} */
