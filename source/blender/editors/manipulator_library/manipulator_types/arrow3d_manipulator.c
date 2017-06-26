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

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_manipulator_library.h"

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

static void manipulator_arrow_matrix_world_get(wmManipulator *mpr, float r_matrix[4][4])
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
			{0.0f, 0.0f, RNA_float_get(arrow->manipulator.ptr, "length")},
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
	float col[4];
	float final_matrix[4][4];

	manipulator_color_get(&arrow->manipulator, highlight, col);
	manipulator_arrow_matrix_world_get(&arrow->manipulator, final_matrix);

	mul_mat3_m4_fl(final_matrix, arrow->manipulator.scale_final);
	mul_m4_m4m4(final_matrix, final_matrix, arrow->manipulator.matrix_offset);

	gpuPushMatrix();
	gpuMultMatrix(final_matrix);
	glEnable(GL_BLEND);
	arrow_draw_geom(arrow, select, col);
	glDisable(GL_BLEND);

	gpuPopMatrix();

	if (arrow->manipulator.interaction_data) {
		ManipulatorInteraction *inter = arrow->manipulator.interaction_data;
		float offset_matrix[4][4];

		copy_m4_m4(offset_matrix, inter->init_matrix);
		mul_mat3_m4_fl(offset_matrix, inter->init_scale);

		gpuPushMatrix();
		gpuMultMatrix(offset_matrix);
		gpuMultMatrix(arrow->manipulator.matrix_offset);

		glEnable(GL_BLEND);
		arrow_draw_geom(arrow, select, (const float [4]){0.5f, 0.5f, 0.5f, 0.5f});
		glDisable(GL_BLEND);

		gpuPopMatrix();
	}
}

static void manipulator_arrow_draw_select(
        const bContext *UNUSED(C), wmManipulator *mpr,
        int selectionbase)
{
	GPU_select_load_id(selectionbase);
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
static void manipulator_arrow_modal(bContext *C, wmManipulator *mpr, const wmEvent *event, const int flag)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	ManipulatorInteraction *inter = mpr->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float orig_origin[4];
	float viewvec[3], tangent[3], plane[3];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float facdir = 1.0f;
	bool use_vertical = false;


	copy_v3_v3(orig_origin, inter->init_matrix[3]);
	orig_origin[3] = 1.0f;
	add_v3_v3v3(offset, orig_origin, arrow->manipulator.matrix_basis[2]);
	offset[3] = 1.0f;

	/* calculate view vector */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}
	normalize_v3(viewvec);

	/* first determine if view vector is really close to the direction. If it is, we use
	 * vertical movement to determine offset, just like transform system does */
	if (RAD2DEG(acos(dot_v3v3(viewvec, arrow->manipulator.matrix_basis[2]))) > 5.0f) {
		/* multiply to projection space */
		mul_m4_v4(rv3d->persmat, orig_origin);
		mul_v4_fl(orig_origin, 1.0f / orig_origin[3]);
		mul_m4_v4(rv3d->persmat, offset);
		mul_v4_fl(offset, 1.0f / offset[3]);

		sub_v2_v2v2(dir_2d, offset, orig_origin);
		dir_2d[0] *= ar->winx;
		dir_2d[1] *= ar->winy;
		normalize_v2(dir_2d);
	}
	else {
		dir_2d[0] = 0.0f;
		dir_2d[1] = 1.0f;
		use_vertical = true;
	}

	/* find mouse difference */
	m_diff[0] = event->mval[0] - inter->init_mval[0];
	m_diff[1] = event->mval[1] - inter->init_mval[1];

	/* project the displacement on the screen space arrow direction */
	project_v2_v2v2(dir2d_final, m_diff, dir_2d);

	float zfac = ED_view3d_calc_zfac(rv3d, orig_origin, NULL);
	ED_view3d_win_to_delta(ar, dir2d_final, offset, zfac);

	add_v3_v3v3(orig_origin, offset, inter->init_matrix[3]);

	/* calculate view vector for the new position */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}

	normalize_v3(viewvec);
	if (!use_vertical) {
		/* now find a plane parallel to the view vector so we can intersect with the arrow direction */
		cross_v3_v3v3(tangent, viewvec, offset);
		cross_v3_v3v3(plane, tangent, viewvec);

		const float plane_offset = dot_v3v3(plane, offset);
		const float plane_dir = dot_v3v3(plane, arrow->manipulator.matrix_basis[2]);
		const float fac = (plane_dir != 0.0f) ? (plane_offset / plane_dir) : 0.0f;
		facdir = (fac < 0.0) ? -1.0 : 1.0;
		if (isfinite(fac)) {
			mul_v3_v3fl(offset, arrow->manipulator.matrix_basis[2], fac);
		}
	}
	else {
		facdir = (m_diff[1] < 0.0) ? -1.0 : 1.0;
	}


	ManipulatorCommonData *data = &arrow->data;
	const float ofs_new = facdir * len_v3(offset);

	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");

	/* set the property for the operator and call its modal function */
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		const int draw_options = RNA_enum_get(arrow->manipulator.ptr, "draw_options");
		const bool constrained = (draw_options & ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED) != 0;
		const bool inverted = (draw_options & ED_MANIPULATOR_ARROW_STYLE_INVERTED) != 0;
		const bool use_precision = (flag & WM_MANIPULATOR_TWEAK_PRECISE) != 0;
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
}

static void manipulator_arrow_setup(wmManipulator *mpr)
{
	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;

	arrow->manipulator.flag |= WM_MANIPULATOR_DRAW_ACTIVE;

	arrow->data.range_fac = 1.0f;
}

static void manipulator_arrow_invoke(
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

	inter->init_scale = mpr->scale_final;

	manipulator_arrow_matrix_world_get(mpr, inter->init_matrix);

	mpr->interaction_data = inter;
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
	if (!cancel)
		return;

	ArrowManipulator3D *arrow = (ArrowManipulator3D *)mpr;
	ManipulatorCommonData *data = &arrow->data;
	ManipulatorInteraction *inter = mpr->interaction_data;

	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	manipulator_property_value_reset(C, mpr, inter, mpr_prop);
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
	wt->matrix_world_get = manipulator_arrow_matrix_world_get;
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
