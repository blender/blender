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

/** \file arrow3d_gizmo.c
 *  \ingroup wm
 *
 * \name Arrow Gizmo
 *
 * 3D Gizmo
 *
 * \brief Simple arrow gizmo which is dragged into a certain direction.
 * The arrow head can have varying shapes, e.g. cone, box, etc.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' for gizmo types that have an up.
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
#include "GPU_state.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_gizmo_library.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

/* to use custom arrows exported to geom_arrow_gizmo.c */
//#define USE_GIZMO_CUSTOM_ARROWS

typedef struct ArrowGizmo3D {
	wmGizmo gizmo;
	GizmoCommonData data;
} ArrowGizmo3D;


/* -------------------------------------------------------------------- */

static void gizmo_arrow_matrix_basis_get(const wmGizmo *gz, float r_matrix[4][4])
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

	copy_m4_m4(r_matrix, arrow->gizmo.matrix_basis);
	madd_v3_v3fl(r_matrix[3], arrow->gizmo.matrix_basis[2], arrow->data.offset);
}

static void arrow_draw_geom(const ArrowGizmo3D *arrow, const bool select, const float color[4])
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	bool unbind_shader = true;
	const int draw_style = RNA_enum_get(arrow->gizmo.ptr, "draw_style");
	const int draw_options = RNA_enum_get(arrow->gizmo.ptr, "draw_options");

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (draw_style == ED_GIZMO_ARROW_STYLE_CROSS) {
		immUniformColor4fv(color);

		immBegin(GPU_PRIM_LINES, 4);
		immVertex3f(pos, -1.0f,  0.0f, 0.0f);
		immVertex3f(pos,  1.0f,  0.0f, 0.0f);
		immVertex3f(pos,  0.0f, -1.0f, 0.0f);
		immVertex3f(pos,  0.0f,  1.0f, 0.0f);
		immEnd();
	}
	else if (draw_style == ED_GIZMO_ARROW_STYLE_CONE) {
		float aspect[2];
		RNA_float_get_array(arrow->gizmo.ptr, "aspect", aspect);
		const float unitx = aspect[0];
		const float unity = aspect[1];
		const float vec[4][3] = {
			{-unitx, -unity, 0},
			{ unitx, -unity, 0},
			{ unitx,  unity, 0},
			{-unitx,  unity, 0},
		};

		GPU_line_width(arrow->gizmo.line_width);
		wm_gizmo_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GPU_PRIM_LINE_LOOP);
	}
	else {
#ifdef USE_GIZMO_CUSTOM_ARROWS
		wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_arrow, select, color);
#else
		const float arrow_length = RNA_float_get(arrow->gizmo.ptr, "length");

		const float vec[2][3] = {
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, arrow_length},
		};

		if (draw_options & ED_GIZMO_ARROW_DRAW_FLAG_STEM) {
			GPU_line_width(arrow->gizmo.line_width);
			wm_gizmo_vec_draw(color, vec, ARRAY_SIZE(vec), pos, GPU_PRIM_LINE_STRIP);
		}
		else {
			immUniformColor4fv(color);
		}

		/* *** draw arrow head *** */

		GPU_matrix_push();

		if (draw_style == ED_GIZMO_ARROW_STYLE_BOX) {
			const float size = 0.05f;

			/* translate to line end with some extra offset so box starts exactly where line ends */
			GPU_matrix_translate_3f(0.0f, 0.0f, arrow_length + size);
			/* scale down to box size */
			GPU_matrix_scale_3f(size, size, size);

			/* draw cube */
			immUnbindProgram();
			unbind_shader = false;
			wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, select, color);
		}
		else {
			BLI_assert(draw_style == ED_GIZMO_ARROW_STYLE_NORMAL);

			const float len = 0.25f;
			const float width = 0.06f;

			/* translate to line end */
			GPU_matrix_translate_3f(0.0f, 0.0f, arrow_length);

			imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
			imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);
		}

		GPU_matrix_pop();
#endif  /* USE_GIZMO_CUSTOM_ARROWS */
	}

	if (unbind_shader) {
		immUnbindProgram();
	}
}

static void arrow_draw_intern(ArrowGizmo3D *arrow, const bool select, const bool highlight)
{
	wmGizmo *gz = &arrow->gizmo;
	float color[4];
	float matrix_final[4][4];

	gizmo_color_get(gz, highlight, color);

	WM_gizmo_calc_matrix_final(gz, matrix_final);

	GPU_matrix_push();
	GPU_matrix_mul(matrix_final);
	GPU_blend(true);
	arrow_draw_geom(arrow, select, color);
	GPU_blend(false);

	GPU_matrix_pop();

	if (gz->interaction_data) {
		GizmoInteraction *inter = gz->interaction_data;

		GPU_matrix_push();
		GPU_matrix_mul(inter->init_matrix_final);


		GPU_blend(true);
		arrow_draw_geom(arrow, select, (const float[4]){0.5f, 0.5f, 0.5f, 0.5f});
		GPU_blend(false);

		GPU_matrix_pop();
	}
}

static void gizmo_arrow_draw_select(
        const bContext *UNUSED(C), wmGizmo *gz,
        int select_id)
{
	GPU_select_load_id(select_id);
	arrow_draw_intern((ArrowGizmo3D *)gz, true, false);
}

static void gizmo_arrow_draw(const bContext *UNUSED(C), wmGizmo *gz)
{
	arrow_draw_intern((ArrowGizmo3D *)gz, false, (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0);
}

/**
 * Calculate arrow offset independent from prop min value,
 * meaning the range will not be offset by min value first.
 */
static int gizmo_arrow_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak tweak_flag)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
	GizmoInteraction *inter = gz->interaction_data;
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
	normalize_v3_v3(arrow_no, arrow->gizmo.matrix_basis[2]);

	int ok = 0;

	for (int j = 0; j < 2; j++) {
		if (ED_view3d_win_to_ray(
		            CTX_data_depsgraph(C),
		            ar, v3d, proj[j].mval,
		            proj[j].ray_origin, proj[j].ray_direction, false))
		{
			/* Force Y axis if we're view aligned */
			if (j == 0) {
				if (RAD2DEGF(acosf(dot_v3v3(proj[j].ray_direction, arrow->gizmo.matrix_basis[2]))) < 5.0f) {
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

	GizmoCommonData *data = &arrow->data;
	const float ofs_new = facdir * len_v3(offset);

	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");

	/* set the property for the operator and call its modal function */
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		const int transform_flag = RNA_enum_get(arrow->gizmo.ptr, "transform");
		const bool constrained = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED) != 0;
		const bool inverted = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_INVERTED) != 0;
		const bool use_precision = (tweak_flag & WM_GIZMO_TWEAK_PRECISE) != 0;
		float value = gizmo_value_from_offset(data, inter, ofs_new, constrained, inverted, use_precision);

		WM_gizmo_target_property_float_set(C, gz, gz_prop, value);
		/* get clamped value */
		value = WM_gizmo_target_property_float_get(gz, gz_prop);

		data->offset = gizmo_offset_from_value(data, value, constrained, inverted);
	}
	else {
		data->offset = ofs_new;
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void gizmo_arrow_setup(wmGizmo *gz)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

	arrow->gizmo.flag |= WM_GIZMO_DRAW_MODAL;

	arrow->data.range_fac = 1.0f;
}

static int gizmo_arrow_invoke(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
	GizmoInteraction *inter = MEM_callocN(sizeof(GizmoInteraction), __func__);
	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");

	/* Some gizmos don't use properties. */
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		inter->init_value = WM_gizmo_target_property_float_get(gz, gz_prop);
	}

	inter->init_offset = arrow->data.offset;

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	gizmo_arrow_matrix_basis_get(gz, inter->init_matrix_basis);
	WM_gizmo_calc_matrix_final(gz, inter->init_matrix_final);

	gz->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static void gizmo_arrow_property_update(wmGizmo *gz, wmGizmoProperty *gz_prop)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
	const int transform_flag = RNA_enum_get(arrow->gizmo.ptr, "transform");
	const bool constrained = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED) != 0;
	const bool inverted = (transform_flag & ED_GIZMO_ARROW_XFORM_FLAG_INVERTED) != 0;
	gizmo_property_data_update(gz, &arrow->data, gz_prop, constrained, inverted);
}

static void gizmo_arrow_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
	GizmoCommonData *data = &arrow->data;
	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	const bool is_prop_valid = WM_gizmo_target_property_is_valid(gz_prop);

	if (!cancel) {
		/* Assign incase applying the opetration needs an updated offset
		 * editmesh bisect needs this. */
		if (is_prop_valid) {
			data->offset = WM_gizmo_target_property_float_get(gz, gz_prop);
		}
		return;
	}

	GizmoInteraction *inter = gz->interaction_data;
	if (is_prop_valid) {
		gizmo_property_value_reset(C, gz, inter, gz_prop);
	}
	data->offset = inter->init_offset;
}


/* -------------------------------------------------------------------- */
/** \name Arrow Gizmo API
 *
 * \{ */

/**
 * Define a custom property UI range
 *
 * \note Needs to be called before WM_gizmo_target_property_def_rna!
 */
void ED_gizmo_arrow3d_set_ui_range(wmGizmo *gz, const float min, const float max)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;

	BLI_assert(min < max);
	BLI_assert(!(WM_gizmo_target_property_is_valid(WM_gizmo_target_property_find(gz, "offset")) &&
	             "Make sure this function is called before WM_gizmo_target_property_def_rna"));

	arrow->data.range = max - min;
	arrow->data.min = min;
	arrow->data.flag |= GIZMO_CUSTOM_RANGE_SET;
}

/**
 * Define a custom factor for arrow min/max distance
 *
 * \note Needs to be called before WM_gizmo_target_property_def_rna!
 */
void ED_gizmo_arrow3d_set_range_fac(wmGizmo *gz, const float range_fac)
{
	ArrowGizmo3D *arrow = (ArrowGizmo3D *)gz;
	BLI_assert(!(WM_gizmo_target_property_is_valid(WM_gizmo_target_property_find(gz, "offset")) &&
	             "Make sure this function is called before WM_gizmo_target_property_def_rna"));

	arrow->data.range_fac = range_fac;
}

static void GIZMO_GT_arrow_3d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_arrow_3d";

	/* api callbacks */
	gzt->draw = gizmo_arrow_draw;
	gzt->draw_select = gizmo_arrow_draw_select;
	gzt->matrix_basis_get = gizmo_arrow_matrix_basis_get;
	gzt->modal = gizmo_arrow_modal;
	gzt->setup = gizmo_arrow_setup;
	gzt->invoke = gizmo_arrow_invoke;
	gzt->property_update = gizmo_arrow_property_update;
	gzt->exit = gizmo_arrow_exit;

	gzt->struct_size = sizeof(ArrowGizmo3D);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style_items[] = {
		{ED_GIZMO_ARROW_STYLE_NORMAL, "NORMAL", 0, "Normal", ""},
		{ED_GIZMO_ARROW_STYLE_CROSS, "CROSS", 0, "Cross", ""},
		{ED_GIZMO_ARROW_STYLE_BOX, "BOX", 0, "Box", ""},
		{ED_GIZMO_ARROW_STYLE_CONE, "CONE", 0, "Cone", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options_items[] = {
		{ED_GIZMO_ARROW_DRAW_FLAG_STEM, "STEM", 0, "Stem", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_transform_items[] = {
		{ED_GIZMO_ARROW_XFORM_FLAG_INVERTED, "INVERT", 0, "Inverted", ""},
		{ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED, "CONSTRAIN", 0, "Constrained", ""},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_enum(
	        gzt->srna, "draw_style", rna_enum_draw_style_items,
	        ED_GIZMO_ARROW_STYLE_NORMAL,
	        "Draw Style", "");
	RNA_def_enum_flag(
	        gzt->srna, "draw_options", rna_enum_draw_options_items,
	        ED_GIZMO_ARROW_DRAW_FLAG_STEM,
	        "Draw Options", "");
	RNA_def_enum_flag(
	        gzt->srna, "transform", rna_enum_transform_items,
	        0,
	        "Transform", "");

	RNA_def_float(gzt->srna, "length", 1.0f, 0.0f, FLT_MAX, "Arrow Line Length", "", 0.0f, FLT_MAX);
	RNA_def_float_vector(gzt->srna, "aspect", 2, NULL, 0, FLT_MAX, "Aspect", "Cone/box style only", 0.0f, FLT_MAX);

	WM_gizmotype_target_property_def(gzt, "offset", PROP_FLOAT, 1);
}

void ED_gizmotypes_arrow_3d(void)
{
	WM_gizmotype_append(GIZMO_GT_arrow_3d);
}

/** \} */
