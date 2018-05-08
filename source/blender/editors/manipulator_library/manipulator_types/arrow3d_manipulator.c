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

/** \file arrow3d_manipulator.c
 *  \ingroup wm
 *
 * \name Arrow Manipulator
 *
 * 3D Manipulator
 *
 * \brief Simple arrow manipulator which is dragged into a certain direction.
 * The arrow head can have varying shapes, e.g. cone, box, etc.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' for manipulator types that have an up.
 * - `matrix[2]` is the arrow direction (for all arrowes).
 */

#include "BIF_gl.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "BKE_context.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_manipulator_library.h"

/* own includes */
#include "../manipulator_geometry.h"
#include "../manipulator_library_intern.h"

/* to use custom arrows exported to geom_arrow_manipulator.c */
//#define USE_MANIPULATOR_CUSTOM_ARROWS

typedef struct ArrowManipulator3D {
	wmManipulator manipulator;
	ManipulatorCommonData data;
} ArrowManipulator3D;


/* -------------------------------------------------------------------- */

static void manipulator_arrow_matrix_basis_get(const wmManipulator *mpr, float r_matrix[4][4])
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;

	copy_m4_m4(r_matrix, arrow->manipulator.matrix_basis);
	madd_v3_v3fl(r_matrix[3], arrow->manipulator.matrix_basis[2], arrow->data.offset);
}

static void arrow_draw_geom(const ArrowManipulator3D *arrow, const bool select, const float color[4])
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	bool unbind_shader = true;
	const int draw_style = RNA_enum_get(arrow->manipulator.ptr, "draw_style");

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (draw_style == ED_MANIPULATOR_ARROW_STYLE_CROSS) {
		immUniformColor4fv(color);

		immBegin(GWN_PRIM_LINES, 4);
		immVertex3f(pos, -1.0f,  0.0f, 0.0f);
		immVertex3f(pos,  1.0f,  0.0f, 0.0f);
		immVertex3f(pos,  0.0f, -1.0f, 0.0f);
		immVertex3f(pos,  0.0f,  1.0f, 0.0f);
		immEnd();
	}
	else if (draw_style == ED_MANIPULATOR_ARROW_STYLE_CONE) {
		float aspect[2];
		RNA_float_get_array(arrow->manipulator.ptr, "aspect", aspect);
		const float unitx = aspect[0];
		const float unity = aspect[1];
		const float vec[4][3] = {
			{-unitx, -unity, 0},
			{ unitx, -unity, 0},
			{ unitx,  unity, 0},
			{-unitx,  unity, 0},
		};

		glLineWidth(arrow->manipulator.line_width);
		wm_manipulator_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GWN_PRIM_LINE_LOOP);
	}
	else {
#ifdef USE_MANIPULATOR_CUSTOM_ARROWS
		wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_arrow, select, color);
#else
		const float arrow_length = RNA_float_get(arrow->manipulator.ptr, "length");

		const float vec[2][3] = {
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, arrow_length},
		};

		glLineWidth(arrow->manipulator.line_width);
		wm_manipulator_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GWN_PRIM_LINE_STRIP);


		/* *** draw arrow head *** */

		gpuPushMatrix();

		if (draw_style == ED_MANIPULATOR_ARROW_STYLE_BOX) {
			const float size = 0.05f;

			/* translate to line end with some extra offset so box starts exactly where line ends */
			gpuTranslate3f(0.0f, 0.0f, arrow_length + size);
			/* scale down to box size */
			gpuScale3f(size, size, size);

			/* draw cube */
			immUnbindProgram();
			unbind_shader = false;
			wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_cube, select, color);
		}
		else {
			BLI_assert(draw_style == ED_MANIPULATOR_ARROW_STYLE_NORMAL);

			const float len = 0.25f;
			const float width = 0.06f;
			const bool use_lighting = (!select && ((U.manipulator_flag & USER_MANIPULATOR_SHADED) != 0));

			/* translate to line end */
			gpuTranslate3f(0.0f, 0.0f, arrow_length);

			if (use_lighting) {
				immUnbindProgram();
				immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);
			}

			imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
			imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);
		}

		gpuPopMatrix();
#endif  /* USE_MANIPULATOR_CUSTOM_ARROWS */
	}

	if (unbind_shader) {
		immUnbindProgram();
	}
}

static void arrow_draw_intern(ArrowManipulator3D *arrow, const bool select, const bool highlight)
{
	wmManipulator *mpr = &arrow->manipulator;
	float color[4];
	float matrix_final[4][4];

	manipulator_color_get(mpr, highlight, color);

	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);
	glEnable(GL_BLEND);
	arrow_draw_geom(arrow, select, color);
	glDisable(GL_BLEND);

	gpuPopMatrix();

	if (mpr->interaction_data) {
		ManipulatorInteraction *inter = mpr->interaction_data;

		gpuPushMatrix();
		gpuMultMatrix(inter->init_matrix_final);


		glEnable(GL_BLEND);
		arrow_draw_geom(arrow, select, (const float[4]){0.5f, 0.5f, 0.5f, 0.5f});
		glDisable(GL_BLEND);

		gpuPopMatrix();
	}
}

static void manipulator_arrow_draw_select(
        const bContext *UNUSED(C), wmManipulator *mpr,
        int select_id)
{
	GPU_select_load_id(select_id);
	arrow_draw_intern((ArrowManipulator3D *)mpr, true, false);
}

static void manipulator_arrow_draw(const bContext *UNUSED(C), wmManipulator *mpr)
{
	arrow_draw_intern((ArrowManipulator3D *)mpr, false, (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0);
}

/**
 * Calculate arrow offset independent from prop min value,
 * meaning the range will not be offset by min value first.
 */
static int manipulator_arrow_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak tweak_flag)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	ManipulatorInteraction *inter = mpr->interaction_data;
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float offset[3];
	float facdir = 1.0f;

	/* (src, dst) */
	struct {
		float mval[2];
		float ray_origin[3], ray_direction[3];
		float location[3];
	} proj[2] = {
		{.mval = {UNPACK2(inter->init_mval)}},
		{.mval = {UNPACK2(event->mval)}},
	};

	float arrow_co[3];
	float arrow_no[3];
	copy_v3_v3(arrow_co, inter->init_matrix_basis[3]);
	normalize_v3_v3(arrow_no, arrow->manipulator.matrix_basis[2]);

	int ok = 0;

	for (int j = 0; j < 2; j++) {
		if (ED_view3d_win_to_ray(
		            CTX_data_depsgraph(C),
		            ar, v3d, proj[j].mval,
		            proj[j].ray_origin, proj[j].ray_direction, false))
		{
			/* Force Y axis if we're view aligned */
			if (j == 0) {
				if (RAD2DEGF(acosf(dot_v3v3(proj[j].ray_direction, arrow->manipulator.matrix_basis[2]))) < 5.0f) {
					normalize_v3_v3(arrow_no, rv3d->viewinv[1]);
				}
			}

			float arrow_no_proj[3];
			project_plane_v3_v3v3(arrow_no_proj, arrow_no, proj[j].ray_direction);

			normalize_v3(arrow_no_proj);

			float plane[4];
			plane_from_point_normal_v3(plane, proj[j].ray_origin, arrow_no_proj);

			float lambda;
			if (isect_ray_plane_v3(arrow_co, arrow_no, plane, &lambda, false)) {
				madd_v3_v3v3fl(proj[j].location, arrow_co, arrow_no, lambda);
				ok++;
			}
		}
	}

	if (ok != 2) {
		return OPERATOR_RUNNING_MODAL;
	}

	sub_v3_v3v3(offset, proj[1].location, proj[0].location);
	facdir = dot_v3v3(arrow_no, offset) < 0.0f ? -1 : 1;

	ManipulatorCommonData *data = &arrow->data;
	const float ofs_new = facdir * len_v3(offset);

	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");

	/* set the property for the operator and call its modal function */
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		const int draw_options = RNA_enum_get(arrow->manipulator.ptr, "draw_options");
		const bool constrained = (draw_options & ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED) != 0;
		const bool inverted = (draw_options & ED_MANIPULATOR_ARROW_STYLE_INVERTED) != 0;
		const bool use_precision = (tweak_flag & WM_MANIPULATOR_TWEAK_PRECISE) != 0;
		float value = manipulator_value_from_offset(data, inter, ofs_new, constrained, inverted, use_precision);

		WM_manipulator_target_property_value_set(C, mpr, mpr_prop, value);
		/* get clamped value */
		value = WM_manipulator_target_property_value_get(mpr, mpr_prop);

		data->offset = manipulator_offset_from_value(data, value, constrained, inverted);
	}
	else {
		data->offset = ofs_new;
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_arrow_setup(wmManipulator *mpr)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;

	arrow->manipulator.flag |= WM_MANIPULATOR_DRAW_MODAL;

	arrow->data.range_fac = 1.0f;
}

static int manipulator_arrow_invoke(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	ManipulatorInteraction *inter = MEM_callocN(sizeof(ManipulatorInteraction), __func__);
	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");

	/* Some manipulators don't use properties. */
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		inter->init_value = WM_manipulator_target_property_value_get(mpr, mpr_prop);
	}

	inter->init_offset = arrow->data.offset;

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	manipulator_arrow_matrix_basis_get(mpr, inter->init_matrix_basis);
	WM_manipulator_calc_matrix_final(mpr, inter->init_matrix_final);

	mpr->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_arrow_property_update(wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	const int draw_options = RNA_enum_get(arrow->manipulator.ptr, "draw_options");
	const bool constrained = (draw_options & ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED) != 0;
	const bool inverted = (draw_options & ED_MANIPULATOR_ARROW_STYLE_INVERTED) != 0;
	manipulator_property_data_update(mpr, &arrow->data, mpr_prop, constrained, inverted);
}

static void manipulator_arrow_exit(bContext *C, wmManipulator *mpr, const bool cancel)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	ManipulatorCommonData *data = &arrow->data;
	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	const bool is_prop_valid = WM_manipulator_target_property_is_valid(mpr_prop);

	if (!cancel) {
		/* Assign incase applying the opetration needs an updated offset
		 * editmesh bisect needs this. */
		if (is_prop_valid) {
			data->offset = WM_manipulator_target_property_value_get(mpr, mpr_prop);
		}
		return;
	}

	ManipulatorInteraction *inter = mpr->interaction_data;
	if (is_prop_valid) {
		manipulator_property_value_reset(C, mpr, inter, mpr_prop);
	}
	data->offset = inter->init_offset;
}


/* -------------------------------------------------------------------- */
/** \name Arrow Manipulator API
 *
 * \{ */

/**
 * Define a custom property UI range
 *
 * \note Needs to be called before WM_manipulator_target_property_def_rna!
 */
void ED_manipulator_arrow3d_set_ui_range(wmManipulator *mpr, const float min, const float max)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;

	BLI_assert(min < max);
	BLI_assert(!(WM_manipulator_target_property_is_valid(WM_manipulator_target_property_find(mpr, "offset")) &&
	             "Make sure this function is called before WM_manipulator_target_property_def_rna"));

	arrow->data.range = max - min;
	arrow->data.min = min;
	arrow->data.flag |= MANIPULATOR_CUSTOM_RANGE_SET;
}

/**
 * Define a custom factor for arrow min/max distance
 *
 * \note Needs to be called before WM_manipulator_target_property_def_rna!
 */
void ED_manipulator_arrow3d_set_range_fac(wmManipulator *mpr, const float range_fac)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	BLI_assert(!(WM_manipulator_target_property_is_valid(WM_manipulator_target_property_find(mpr, "offset")) &&
	             "Make sure this function is called before WM_manipulator_target_property_def_rna"));

	arrow->data.range_fac = range_fac;
}

static void MANIPULATOR_WT_arrow_3d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_arrow_3d";

	/* api callbacks */
	wt->draw = manipulator_arrow_draw;
	wt->draw_select = manipulator_arrow_draw_select;
	wt->matrix_basis_get = manipulator_arrow_matrix_basis_get;
	wt->modal = manipulator_arrow_modal;
	wt->setup = manipulator_arrow_setup;
	wt->invoke = manipulator_arrow_invoke;
	wt->property_update = manipulator_arrow_property_update;
	wt->exit = manipulator_arrow_exit;

	wt->struct_size = sizeof(ArrowManipulator3D);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_MANIPULATOR_ARROW_STYLE_NORMAL, "NORMAL", 0, "Normal", ""},
		{ED_MANIPULATOR_ARROW_STYLE_CROSS, "CROSS", 0, "Cross", ""},
		{ED_MANIPULATOR_ARROW_STYLE_BOX, "BOX", 0, "Box", ""},
		{ED_MANIPULATOR_ARROW_STYLE_CONE, "CONE", 0, "Cone", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_ARROW_STYLE_INVERTED, "INVERT", 0, "Inverted", ""},
		{ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED, "CONSTRAIN", 0, "Constrained", ""},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_enum(wt->srna, "draw_style", rna_enum_draw_style, ED_MANIPULATOR_ARROW_STYLE_NORMAL, "Draw Style", "");
	RNA_def_enum_flag(wt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	RNA_def_float(wt->srna, "length", 1.0f, 0.0f, FLT_MAX, "Arrow Line Length", "", 0.0f, FLT_MAX);
	RNA_def_float_vector(wt->srna, "aspect", 2, NULL, 0, FLT_MAX, "Aspect", "Cone/box style only", 0.0f, FLT_MAX);

	WM_manipulatortype_target_property_def(wt, "offset", PROP_FLOAT, 1);
}

void ED_manipulatortypes_arrow_3d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_arrow_3d);
}

/** \} */
