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

/** \file cage3d_gizmo.c
 *  \ingroup wm
 *
 * \name Cage Gizmo
 *
 * 2D Gizmo
 *
 * \brief Rectangular gizmo acting as a 'cage' around its content.
 * Interacting scales or translates the gizmo.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"

/* own includes */
#include "../gizmo_library_intern.h"

#define GIZMO_RESIZER_SIZE 10.0f
#define GIZMO_MARGIN_OFFSET_SCALE 1.5f

static void gizmo_calc_matrix_final_no_offset(
        const wmGizmo *gz, float orig_matrix_final_no_offset[4][4], bool use_space)
{
	float mat_identity[4][4];
	struct WM_GizmoMatrixParams params = {NULL};
	unit_m4(mat_identity);
	if (use_space == false) {
		params.matrix_basis = mat_identity;
	}
	params.matrix_offset = mat_identity;
	WM_gizmo_calc_matrix_final_params(gz, &params, orig_matrix_final_no_offset);
}

static void gizmo_calc_rect_view_scale(
        const wmGizmo *gz, const float dims[3], float scale[3])
{
	UNUSED_VARS(dims);

	/* Unlike cage2d, no need to correct for aspect. */
	float matrix_final_no_offset[4][4];

	float x_axis[3], y_axis[3], z_axis[3];
	gizmo_calc_matrix_final_no_offset(gz, matrix_final_no_offset, false);
	mul_v3_mat3_m4v3(x_axis, matrix_final_no_offset, gz->matrix_offset[0]);
	mul_v3_mat3_m4v3(y_axis, matrix_final_no_offset, gz->matrix_offset[1]);
	mul_v3_mat3_m4v3(z_axis, matrix_final_no_offset, gz->matrix_offset[2]);

	scale[0] = 1.0f / len_v3(x_axis);
	scale[1] = 1.0f / len_v3(y_axis);
	scale[2] = 1.0f / len_v3(z_axis);
}

static void gizmo_calc_rect_view_margin(
        const wmGizmo *gz, const float dims[3], float margin[3])
{
	float handle_size;
	if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
		handle_size = 0.15f;
	}
	else {
		handle_size = GIZMO_RESIZER_SIZE;
	}
	// XXX, the scale isn't taking offset into account, we need to calculate scale per handle!
	// handle_size *= gz->scale_final;

	float scale_xyz[3];
	gizmo_calc_rect_view_scale(gz, dims, scale_xyz);
	margin[0] = ((handle_size * scale_xyz[0]));
	margin[1] = ((handle_size * scale_xyz[1]));
	margin[2] = ((handle_size * scale_xyz[2]));
}

/* -------------------------------------------------------------------- */

static void gizmo_rect_pivot_from_scale_part(int part, float r_pt[3], bool r_constrain_axis[3])
{
	if (part >= ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
	    part <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
	{
		int index = (part - ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
		int range[3];
		range[2] = index % 3;
		index    = index / 3;
		range[1] = index % 3;
		index    = index / 3;
		range[0] = index % 3;

		const float sign[3] = {0.5f, 0.0f, -0.5f};
		for (int i = 0; i < 3; i++) {
			r_pt[i] = sign[range[i]];
			r_constrain_axis[i] = (range[i] == 1);
		}
	}
}

/* -------------------------------------------------------------------- */
/** \name Box Draw Style
 *
 * Useful for 3D views, see: #ED_GIZMO_CAGE2D_STYLE_BOX
 * \{ */

static void cage3d_draw_box_corners(
        const float r[3], const float margin[3], const float color[3])
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	UNUSED_VARS(margin);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	imm_draw_cube_wire_3d(pos, (float[3]){0}, r);

	immUnbindProgram();
}

static void cage3d_draw_box_interaction(
        const float color[4], const int highlighted,
        const float size[3], const float margin[3])
{
	if (highlighted >= ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
	    highlighted <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
	{
		int index = (highlighted - ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
		int range[3];
		range[2] = index % 3;
		index    = index / 3;
		range[1] = index % 3;
		index    = index / 3;
		range[0] = index % 3;

		const float sign[3] = {-1.0f, 0.0f, 1.0f};
		float co[3];

		for (int i = 0; i < 3; i++) {
			co[i] = size[i] * sign[range[i]];
		}
		const float rad[3] = {margin[0] / 3, margin[1] / 3, margin[2] / 3};

		{
			uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			immUniformColor3fv(color);
			imm_draw_cube_fill_3d(pos, co, rad);
			immUnbindProgram();
		}
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Draw Style
 *
 * Useful for 2D views, see: #ED_GIZMO_CAGE2D_STYLE_CIRCLE
 * \{ */

static void imm_draw_point_aspect_3d(
        uint pos, const float co[3], const float rad[3], bool solid)
{
	if (solid) {
		imm_draw_cube_fill_3d(pos, co, rad);
	}
	else {
		imm_draw_cube_wire_3d(pos, co, rad);
	}
}

static void cage3d_draw_circle_wire(
        const float r[3], const float margin[3], const float color[3],
        const int transform_flag, const int draw_options)
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	imm_draw_cube_wire_3d(pos, (float[3]){0}, r);

#if 0
	if (transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE) {
		if (draw_options & ED_GIZMO_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
			const float rad[2] = {margin[0] / 2, margin[1] / 2};
			const float center[2] = {0.0f, 0.0f};

			immBegin(GPU_PRIM_LINES, 4);
			immVertex2f(pos, center[0] - rad[0], center[1] - rad[1]);
			immVertex2f(pos, center[0] + rad[0], center[1] + rad[1]);
			immVertex2f(pos, center[0] + rad[0], center[1] - rad[1]);
			immVertex2f(pos, center[0] - rad[0], center[1] + rad[1]);
			immEnd();
		}
	}
#else
	UNUSED_VARS(margin, transform_flag, draw_options);
#endif


	immUnbindProgram();
}

static void cage3d_draw_circle_handles(
        const RegionView3D *rv3d, const float matrix_final[4][4],
        const float r[3], const float margin[3], const float color[3],
        bool solid, float scale)
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	const float rad[3] = {margin[0] / 3, margin[1] / 3, margin[2] / 3};

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	float sign[3] = {-1.0f, 0.0f, 1.0f};
	for (int x = 0; x < 3; x++) {
		for (int y = 0; y < 3; y++) {
			for (int z = 0; z < 3; z++) {
				if (x == 1 && y == 1 && z == 1) {
					continue;
				}
				const float co[3] = {r[0] * sign[x], r[1] * sign[y], r[2] * sign[z]};
				float co_test[3];
				mul_v3_m4v3(co_test, matrix_final, co);
				float rad_scale[3];
				mul_v3_v3fl(rad_scale, rad, ED_view3d_pixel_size(rv3d, co_test) * scale);
				imm_draw_point_aspect_3d(pos, co, rad_scale, solid);
			}
		}
	}

	immUnbindProgram();
}

/** \} */

static void gizmo_cage3d_draw_intern(
        RegionView3D *rv3d,
        wmGizmo *gz, const bool select, const bool highlight, const int select_id)
{
	// const bool use_clamp = (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) == 0;
	float dims[3];
	RNA_float_get_array(gz->ptr, "dimensions", dims);
	float matrix_final[4][4];

	const int transform_flag = RNA_enum_get(gz->ptr, "transform");
	const int draw_style = RNA_enum_get(gz->ptr, "draw_style");
	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");

	const float size_real[3] = {dims[0] / 2.0f, dims[1] / 2.0f, dims[2] / 2.0f};

	WM_gizmo_calc_matrix_final(gz, matrix_final);

	GPU_matrix_push();
	GPU_matrix_mul(matrix_final);

	float margin[3];
	gizmo_calc_rect_view_margin(gz, dims, margin);

	/* Handy for quick testing draw (if it's outside bounds). */
	if (false) {
		GPU_blend(true);
		uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv((const float[4]){1, 1, 1, 0.5f});
		float s = 0.5f;
		immRectf(pos, -s, -s, s, s);
		immUnbindProgram();
		GPU_blend(false);
	}

	if (select) {
		/* expand for hotspot */
#if 0
		const float size[3] = {
		    size_real[0] + margin[0] / 2,
		    size_real[1] + margin[1] / 2,
		    size_real[2] + margin[2] / 2,
		};
#else
		/* just use same value for now. */
		const float size[3] = {UNPACK3(size_real)};
#endif


		if (transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE) {
			for (int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
			     i <= ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z;
			     i++)
			{
				if (i == ED_GIZMO_CAGE3D_PART_SCALE_MID_X_MID_Y_MID_Z) {
					continue;
				}
				GPU_select_load_id(select_id | i);
				cage3d_draw_box_interaction(
				        gz->color, i, size, margin);
			}
		}
		if (transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE) {
			const int transform_part = ED_GIZMO_CAGE3D_PART_TRANSLATE;
			GPU_select_load_id(select_id | transform_part);
			cage3d_draw_box_interaction(
			        gz->color, transform_part, size, margin);
		}
	}
	else {
#if 0
		const rctf _r = {
			.xmin = -size_real[0],
			.ymin = -size_real[1],
			.xmax = size_real[0],
			.ymax = size_real[1],
		};
#endif
		if (draw_style == ED_GIZMO_CAGE2D_STYLE_BOX) {
			/* corner gizmos */
			GPU_line_width(gz->line_width + 3.0f);
			cage3d_draw_box_corners(size_real, margin, (const float[3]){0, 0, 0});

			/* corner gizmos */
			float color[4];
			gizmo_color_get(gz, highlight, color);
			GPU_line_width(gz->line_width);
			cage3d_draw_box_corners(size_real, margin, color);

			bool show = false;
			if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_TRANSLATE) {
				/* Only show if we're drawing the center handle
				 * otherwise the entire rectangle is the hotspot. */
				if (draw_options & ED_GIZMO_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
					show = true;
				}
			}
			else {
				show = true;
			}

			if (show) {
				cage3d_draw_box_interaction(
				        gz->color, gz->highlight_part, size_real, margin);
			}
		}
		else if (draw_style == ED_GIZMO_CAGE2D_STYLE_CIRCLE) {
			float color[4];
			gizmo_color_get(gz, highlight, color);

			GPU_line_smooth(true);
			GPU_polygon_smooth(true);
			GPU_blend(true);

			GPU_line_width(gz->line_width + 3.0f);
			cage3d_draw_circle_wire(size_real, margin, (const float[3]){0, 0, 0}, transform_flag, draw_options);
			GPU_line_width(gz->line_width);
			cage3d_draw_circle_wire(size_real, margin, color, transform_flag, draw_options);

			/* corner gizmos */
			cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, (const float[3]){0, 0, 0}, true, 60);
			cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, color, true, 40);

			GPU_blend(false);
			GPU_polygon_smooth(false);
			GPU_line_smooth(false);
		}
		else {
			BLI_assert(0);
		}
	}

	GPU_line_width(1.0);
	GPU_matrix_pop();
}

/**
 * For when we want to draw 3d cage in 3d views.
 */
static void gizmo_cage3d_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	gizmo_cage3d_draw_intern(rv3d, gz, true, false, select_id);
}

static void gizmo_cage3d_draw(const bContext *C, wmGizmo *gz)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;
	gizmo_cage3d_draw_intern(rv3d, gz, false, is_highlight, -1);
}

static int gizmo_cage3d_get_cursor(wmGizmo *gz)
{
	if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
		return BC_NSEW_SCROLLCURSOR;
	}

	return CURSOR_STD;
}

typedef struct RectTransformInteraction {
	float orig_mouse[3];
	float orig_matrix_offset[4][4];
	float orig_matrix_final_no_offset[4][4];
} RectTransformInteraction;

static void gizmo_cage3d_setup(wmGizmo *gz)
{
	gz->flag |= /* WM_GIZMO_DRAW_MODAL | */ /* TODO */
	             WM_GIZMO_DRAW_NO_SCALE;
}

static int gizmo_cage3d_invoke(
        bContext *C, wmGizmo *gz, const wmEvent *event)
{
	RectTransformInteraction *data = MEM_callocN(sizeof(RectTransformInteraction), "cage_interaction");

	copy_m4_m4(data->orig_matrix_offset, gz->matrix_offset);
	gizmo_calc_matrix_final_no_offset(gz, data->orig_matrix_final_no_offset, true);

	if (gizmo_window_project_3d(
	        C, gz, (const float[2]){UNPACK2(event->mval)}, false, data->orig_mouse) == 0)
	{
		zero_v3(data->orig_mouse);
	}

	gz->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static int gizmo_cage3d_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
	/* For transform logic to be managable we operate in -0.5..0.5 2D space,
	 * no matter the size of the rectangle, mouse coorts are scaled to unit space.
	 * The mouse coords have been projected into the matrix so we don't need to worry about axis alignment.
	 *
	 * - The cursor offset are multiplied by 'dims'.
	 * - Matrix translation is also multiplied by 'dims'.
	 */
	RectTransformInteraction *data = gz->interaction_data;
	float point_local[3];

	float dims[3];
	RNA_float_get_array(gz->ptr, "dimensions", dims);

	{
		float matrix_back[4][4];
		copy_m4_m4(matrix_back, gz->matrix_offset);
		copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);

		bool ok = gizmo_window_project_3d(
		        C, gz, (const float[2]){UNPACK2(event->mval)}, false, point_local);
		copy_m4_m4(gz->matrix_offset, matrix_back);
		if (!ok) {
			return OPERATOR_RUNNING_MODAL;
		}
	}

	const int transform_flag = RNA_enum_get(gz->ptr, "transform");
	wmGizmoProperty *gz_prop;

	gz_prop = WM_gizmo_target_property_find(gz, "matrix");
	if (gz_prop->type != NULL) {
		WM_gizmo_target_property_value_get_array(gz, gz_prop, &gz->matrix_offset[0][0]);
	}

	if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_TRANSLATE) {
		/* do this to prevent clamping from changing size */
		copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);
		gz->matrix_offset[3][0] = data->orig_matrix_offset[3][0] + (point_local[0] - data->orig_mouse[0]);
		gz->matrix_offset[3][1] = data->orig_matrix_offset[3][1] + (point_local[1] - data->orig_mouse[1]);
		gz->matrix_offset[3][2] = data->orig_matrix_offset[3][2] + (point_local[2] - data->orig_mouse[2]);
	}
	else if (gz->highlight_part == ED_GIZMO_CAGE3D_PART_ROTATE) {
		/* TODO (if needed) */
	}
	else {
		/* scale */
		copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);
		float pivot[3];
		bool constrain_axis[3] = {false};

		if (transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE) {
			gizmo_rect_pivot_from_scale_part(gz->highlight_part, pivot, constrain_axis);
		}
		else {
			zero_v3(pivot);
		}

		/* Cursor deltas scaled to (-0.5..0.5). */
		float delta_orig[3], delta_curr[3];

		for (int i = 0; i < 3; i++) {
			delta_orig[i] = ((data->orig_mouse[i] - data->orig_matrix_offset[3][i]) / dims[i]) - pivot[i];
			delta_curr[i] = ((point_local[i]      - data->orig_matrix_offset[3][i]) / dims[i]) - pivot[i];
		}

		float scale[3] = {1.0f, 1.0f, 1.0f};
		for (int i = 0; i < 3; i++) {
			if (constrain_axis[i] == false) {
				if (delta_orig[i] < 0.0f) {
					delta_orig[i] *= -1.0f;
					delta_curr[i] *= -1.0f;
				}
				const int sign = signum_i(scale[i]);

				scale[i] = 1.0f + ((delta_curr[i] - delta_orig[i]) / len_v3(data->orig_matrix_offset[i]));

				if ((transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_SIGNED) == 0) {
					if (sign != signum_i(scale[i])) {
						scale[i] = 0.0f;
					}
				}
			}
		}

		if (transform_flag & ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM) {
			if (constrain_axis[0] == false && constrain_axis[1] == false) {
				scale[1] = scale[0] = (scale[1] + scale[0]) / 2.0f;
			}
			else if (constrain_axis[0] == false) {
				scale[1] = scale[0];
			}
			else if (constrain_axis[1] == false) {
				scale[0] = scale[1];
			}
			else {
				BLI_assert(0);
			}
		}

		/* scale around pivot */
		float matrix_scale[4][4];
		unit_m4(matrix_scale);

		mul_v3_fl(matrix_scale[0], scale[0]);
		mul_v3_fl(matrix_scale[1], scale[1]);
		mul_v3_fl(matrix_scale[2], scale[2]);

		transform_pivot_set_m4(
		        matrix_scale,
		        (const float[3]){pivot[0] * dims[0], pivot[1] * dims[1], pivot[2] * dims[2]});
		mul_m4_m4m4(gz->matrix_offset, data->orig_matrix_offset, matrix_scale);
	}

	if (gz_prop->type != NULL) {
		WM_gizmo_target_property_value_set_array(C, gz, gz_prop, &gz->matrix_offset[0][0]);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(CTX_wm_region(C));
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void gizmo_cage3d_property_update(wmGizmo *gz, wmGizmoProperty *gz_prop)
{
	if (STREQ(gz_prop->type->idname, "matrix")) {
		if (WM_gizmo_target_property_array_length(gz, gz_prop) == 16) {
			WM_gizmo_target_property_value_get_array(gz, gz_prop, &gz->matrix_offset[0][0]);
		}
		else {
			BLI_assert(0);
		}
	}
	else {
		BLI_assert(0);
	}
}

static void gizmo_cage3d_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
	RectTransformInteraction *data = gz->interaction_data;

	if (!cancel)
		return;

	wmGizmoProperty *gz_prop;

	/* reset properties */
	gz_prop = WM_gizmo_target_property_find(gz, "matrix");
	if (gz_prop->type != NULL) {
		WM_gizmo_target_property_value_set_array(C, gz, gz_prop, &data->orig_matrix_offset[0][0]);
	}

	copy_m4_m4(gz->matrix_offset, data->orig_matrix_offset);
}


/* -------------------------------------------------------------------- */
/** \name Cage Gizmo API
 *
 * \{ */

static void GIZMO_GT_cage_3d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_cage_3d";

	/* api callbacks */
	gzt->draw = gizmo_cage3d_draw;
	gzt->draw_select = gizmo_cage3d_draw_select;
	gzt->setup = gizmo_cage3d_setup;
	gzt->invoke = gizmo_cage3d_invoke;
	gzt->property_update = gizmo_cage3d_property_update;
	gzt->modal = gizmo_cage3d_modal;
	gzt->exit = gizmo_cage3d_exit;
	gzt->cursor_get = gizmo_cage3d_get_cursor;

	gzt->struct_size = sizeof(wmGizmo);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_GIZMO_CAGE2D_STYLE_BOX, "BOX", 0, "Box", ""},
		{ED_GIZMO_CAGE2D_STYLE_CIRCLE, "CIRCLE", 0, "Circle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_transform[] = {
		{ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
		{ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE, "SCALE", 0, "Scale", ""},
		{ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM, "SCALE_UNIFORM", 0, "Scale Uniform", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_GIZMO_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "XFORM_CENTER_HANDLE", 0, "Center Handle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static float unit_v3[3] = {1.0f, 1.0f, 1.0f};
	RNA_def_float_vector(gzt->srna, "dimensions", 3, unit_v3, 0, FLT_MAX, "Dimensions", "", 0.0f, FLT_MAX);
	RNA_def_enum_flag(gzt->srna, "transform", rna_enum_transform, 0, "Transform Options", "");
	RNA_def_enum(gzt->srna, "draw_style", rna_enum_draw_style, ED_GIZMO_CAGE2D_STYLE_CIRCLE, "Draw Style", "");
	RNA_def_enum_flag(
	        gzt->srna, "draw_options", rna_enum_draw_options,
	        ED_GIZMO_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "Draw Options", "");

	WM_gizmotype_target_property_def(gzt, "matrix", PROP_FLOAT, 16);
}

void ED_gizmotypes_cage_3d(void)
{
	WM_gizmotype_append(GIZMO_GT_cage_3d);
}

/** \} */
