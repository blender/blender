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

/** \file blender/editors/transform/transform_manipulator2d.c
 *  \ingroup edtransform
 *
 * \name 2D Transform Manipulator
 *
 * Used for UV/Image Editor
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "RNA_access.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h" /* XXX */

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_manipulator_library.h"

#include "transform.h" /* own include */

/* axes as index */
enum {
	MAN2D_AXIS_TRANS_X = 0,
	MAN2D_AXIS_TRANS_Y,

	MAN2D_AXIS_LAST,
};

typedef struct ManipulatorGroup2D {
	wmManipulator *translate_x,
	         *translate_y;

	wmManipulator *cage;

	/* Current origin in view space, used to update widget origin for possible view changes */
	float origin[2];
	float min[2];
	float max[2];

} ManipulatorGroup2D;


/* **************** Utilities **************** */

/* loop over axes */
#define MAN2D_ITER_AXES_BEGIN(axis, axis_idx) \
	{ \
		wmManipulator *axis; \
		int axis_idx; \
		for (axis_idx = 0; axis_idx < MAN2D_AXIS_LAST; axis_idx++) { \
			axis = manipulator2d_get_axis_from_index(man, axis_idx);

#define MAN2D_ITER_AXES_END \
		} \
	} ((void)0)

static wmManipulator *manipulator2d_get_axis_from_index(const ManipulatorGroup2D *man, const short axis_idx)
{
	BLI_assert(IN_RANGE_INCL(axis_idx, (float)MAN2D_AXIS_TRANS_X, (float)MAN2D_AXIS_TRANS_Y));

	switch (axis_idx) {
		case MAN2D_AXIS_TRANS_X:
			return man->translate_x;
		case MAN2D_AXIS_TRANS_Y:
			return man->translate_y;
	}

	return NULL;
}

static void manipulator2d_get_axis_color(const int axis_idx, float *r_col, float *r_col_hi)
{
	const float alpha = 0.6f;
	const float alpha_hi = 1.0f;
	int col_id;

	switch (axis_idx) {
		case MAN2D_AXIS_TRANS_X:
			col_id = TH_AXIS_X;
			break;
		case MAN2D_AXIS_TRANS_Y:
			col_id = TH_AXIS_Y;
			break;
	}

	UI_GetThemeColor4fv(col_id, r_col);

	copy_v4_v4(r_col_hi, r_col);
	r_col[3] *= alpha;
	r_col_hi[3] *= alpha_hi;
}

static ManipulatorGroup2D *manipulatorgroup2d_init(wmManipulatorGroup *mgroup)
{
	const wmManipulatorType *wt_arrow = WM_manipulatortype_find("MANIPULATOR_WT_arrow_2d", true);
	const wmManipulatorType *wt_cage = WM_manipulatortype_find("MANIPULATOR_WT_cage_2d", true);

	ManipulatorGroup2D *man = MEM_callocN(sizeof(ManipulatorGroup2D), __func__);

	man->translate_x = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
	man->translate_y = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
	man->cage = WM_manipulator_new_ptr(wt_cage, mgroup, NULL);

	RNA_enum_set(man->cage->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE |
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE |
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE);

	return man;
}

/**
 * Calculates origin in view space, use with #manipulator2d_origin_to_region.
 */
static void manipulator2d_calc_bounds(const bContext *C, float *r_center, float *r_min, float *r_max)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = ED_space_image(sima);

	float min_buf[2], max_buf[2];
	if (r_min == NULL) {
		r_min = min_buf;
	}
	if (r_max == NULL) {
		r_max = max_buf;
	}

	if (!ED_uvedit_minmax(CTX_data_scene(C), ima, CTX_data_edit_object(C), r_min, r_max)) {
		zero_v2(r_min);
		zero_v2(r_max);
	}
	mid_v2_v2v2(r_center, r_min, r_max);
}

/**
 * Convert origin (or any other point) from view to region space.
 */
BLI_INLINE void manipulator2d_origin_to_region(ARegion *ar, float *r_origin)
{
	UI_view2d_view_to_region_fl(&ar->v2d, r_origin[0], r_origin[1], &r_origin[0], &r_origin[1]);
}

/**
 * Custom handler for manipulator widgets
 */
static int manipulator2d_modal(
        bContext *C, wmManipulator *widget, const wmEvent *UNUSED(event),
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	ARegion *ar = CTX_wm_region(C);
	float origin[3];

	manipulator2d_calc_bounds(C, origin, NULL, NULL);
	manipulator2d_origin_to_region(ar, origin);
	WM_manipulator_set_matrix_location(widget, origin);

	ED_region_tag_redraw(ar);

	return OPERATOR_RUNNING_MODAL;
}

void ED_widgetgroup_manipulator2d_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmOperatorType *ot_translate = WM_operatortype_find("TRANSFORM_OT_translate", true);
	ManipulatorGroup2D *man = manipulatorgroup2d_init(mgroup);
	mgroup->customdata = man;

	MAN2D_ITER_AXES_BEGIN(axis, axis_idx)
	{
		const float offset[3] = {0.0f, 0.2f};

		float color[4], color_hi[4];
		manipulator2d_get_axis_color(axis_idx, color, color_hi);

		/* custom handler! */
		WM_manipulator_set_fn_custom_modal(axis, manipulator2d_modal);
		/* set up widget data */
		RNA_float_set(axis->ptr, "angle", -M_PI_2 * axis_idx);
		RNA_float_set(axis->ptr, "length", 0.8f);
		WM_manipulator_set_matrix_offset_location(axis, offset);
		WM_manipulator_set_line_width(axis, MANIPULATOR_AXIS_LINE_WIDTH);
		WM_manipulator_set_scale(axis, U.manipulator_size);
		WM_manipulator_set_color(axis, color);
		WM_manipulator_set_color_highlight(axis, color_hi);

		/* assign operator */
		PointerRNA *ptr = WM_manipulator_operator_set(axis, 0, ot_translate, NULL);
		int constraint[3] = {0};
		constraint[(axis_idx + 1) % 2] = 1;
		if (RNA_struct_find_property(ptr, "constraint_axis"))
			RNA_boolean_set_array(ptr, "constraint_axis", constraint);
		RNA_boolean_set(ptr, "release_confirm", 1);
	}
	MAN2D_ITER_AXES_END;

	{
		wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
		wmOperatorType *ot_rotate = WM_operatortype_find("TRANSFORM_OT_rotate", true);
		PointerRNA *ptr;

		/* assign operator */
		ptr = WM_manipulator_operator_set(man->cage, 0, ot_translate, NULL);
		RNA_boolean_set(ptr, "release_confirm", 1);

		int constraint_x[3] = {1, 0, 0};
		int constraint_y[3] = {0, 1, 0};

		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X, ot_resize, NULL);
		PropertyRNA *prop_release_confirm = RNA_struct_find_property(ptr, "release_confirm");
		PropertyRNA *prop_constraint_axis = RNA_struct_find_property(ptr, "constraint_axis");
		RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_x);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X, ot_resize, NULL);
		RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_x);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y, ot_resize, NULL);
		RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_y);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y, ot_resize, NULL);
		RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint_y);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);

		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y, ot_resize, NULL);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y, ot_resize, NULL);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y, ot_resize, NULL);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y, ot_resize, NULL);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
		ptr = WM_manipulator_operator_set(man->cage, ED_MANIPULATOR_CAGE2D_PART_ROTATE, ot_rotate, NULL);
		RNA_property_boolean_set(ptr, prop_release_confirm, true);
	}
}

void ED_widgetgroup_manipulator2d_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	ManipulatorGroup2D *man = mgroup->customdata;
	float origin[3];
	manipulator2d_calc_bounds(C, origin, man->min, man->max);
	copy_v2_v2(man->origin, origin);
	bool show_cage = !equals_v2v2(man->min, man->max);

	if (show_cage) {
		man->cage->flag &= ~WM_MANIPULATOR_HIDDEN;
		man->translate_x->flag |= WM_MANIPULATOR_HIDDEN;
		man->translate_y->flag |= WM_MANIPULATOR_HIDDEN;
	}
	else {
		man->cage->flag |= WM_MANIPULATOR_HIDDEN;
		man->translate_x->flag &= ~WM_MANIPULATOR_HIDDEN;
		man->translate_y->flag &= ~WM_MANIPULATOR_HIDDEN;
	}

	if (show_cage) {
		wmManipulatorOpElem *mpop;
		float mid[2];
		const float *min = man->min;
		const float *max = man->max;
		mid_v2_v2v2(mid, min, max);

		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X);
		PropertyRNA *prop_center_override = RNA_struct_find_property(&mpop->ptr, "center_override");
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){max[0], mid[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){min[0], mid[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){mid[0], max[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){mid[0], min[1], 0.0f});

		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){max[0], max[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){max[0], min[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){min[0], max[1], 0.0f});
		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){min[0], min[1], 0.0f});

		mpop = WM_manipulator_operator_get(man->cage, ED_MANIPULATOR_CAGE2D_PART_ROTATE);
		RNA_property_float_set_array(&mpop->ptr, prop_center_override, (float[3]){mid[0], mid[1], 0.0f});
	}
}

void ED_widgetgroup_manipulator2d_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	ARegion *ar = CTX_wm_region(C);
	ManipulatorGroup2D *man = mgroup->customdata;
	float origin[3] = {UNPACK2(man->origin), 0.0f};
	float origin_aa[3] = {UNPACK2(man->origin), 0.0f};

	manipulator2d_origin_to_region(ar, origin);

	MAN2D_ITER_AXES_BEGIN(axis, axis_idx)
	{
		WM_manipulator_set_matrix_location(axis, origin);
	}
	MAN2D_ITER_AXES_END;

	UI_view2d_view_to_region_m4(&ar->v2d, man->cage->matrix_space);
	WM_manipulator_set_matrix_offset_location(man->cage, origin_aa);
	man->cage->matrix_offset[0][0] = (man->max[0] - man->min[0]);
	man->cage->matrix_offset[1][1] = (man->max[1] - man->min[1]);
}

/* TODO (Julian)
 * - Called on every redraw, better to do a more simple poll and check for selection in _refresh
 * - UV editing only, could be expanded for other things.
 */
bool ED_widgetgroup_manipulator2d_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	if ((U.manipulator_flag & USER_MANIPULATOR_DRAW) == 0) {
		return false;
	}

	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);

	if (ED_space_image_show_uvedit(sima, obedit)) {
		Image *ima = ED_space_image(sima);
		Scene *scene = CTX_data_scene(C);
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		BMFace *efa;
		BMLoop *l;
		BMIter iter, liter;

		const int cd_loop_uv_offset  = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

		/* check if there's a selected poly */
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (!uvedit_face_visible_test(scene, obedit, ima, efa))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
					return true;
				}
			}
		}
	}

	return false;
}
