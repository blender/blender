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

/** \file cage3d_manipulator.c
 *  \ingroup wm
 *
 * \name Cage Manipulator
 *
 * 2D Manipulator
 *
 * \brief Rectangular manipulator acting as a 'cage' around its content.
 * Interacting scales or translates the manipulator.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_dial_2d.h"
#include "BLI_rect.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

/* own includes */
#include "../manipulator_library_intern.h"

#define MANIPULATOR_RESIZER_SIZE 10.0f
#define MANIPULATOR_MARGIN_OFFSET_SCALE 1.5f

static void manipulator_calc_matrix_final_no_offset(
        const wmManipulator *mpr, float orig_matrix_final_no_offset[4][4], bool use_space)
{
	float mat_identity[4][4];
	struct WM_ManipulatorMatrixParams params = {NULL};
	unit_m4(mat_identity);
	if (use_space == false) {
		params.matrix_basis = mat_identity;
	}
	params.matrix_offset = mat_identity;
	WM_manipulator_calc_matrix_final_params(mpr, &params, orig_matrix_final_no_offset);
}

static void manipulator_calc_rect_view_scale(
        const wmManipulator *mpr, const float dims[3], float scale[3])
{
	UNUSED_VARS(dims);

	/* Unlike cage2d, no need to correct for aspect. */
	float matrix_final_no_offset[4][4];

	float x_axis[3], y_axis[3], z_axis[3];
	manipulator_calc_matrix_final_no_offset(mpr, matrix_final_no_offset, false);
	mul_v3_mat3_m4v3(x_axis, matrix_final_no_offset, mpr->matrix_offset[0]);
	mul_v3_mat3_m4v3(y_axis, matrix_final_no_offset, mpr->matrix_offset[1]);
	mul_v3_mat3_m4v3(z_axis, matrix_final_no_offset, mpr->matrix_offset[2]);

	scale[0] = 1.0f / len_v3(x_axis);
	scale[1] = 1.0f / len_v3(y_axis);
	scale[2] = 1.0f / len_v3(z_axis);
}

static void manipulator_calc_rect_view_margin(
        const wmManipulator *mpr, const float dims[3], float margin[3])
{
	float handle_size;
	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		handle_size = 0.15f;
	}
	else {
		handle_size = MANIPULATOR_RESIZER_SIZE;
	}
	// XXX, the scale isn't taking offset into account, we need to calculate scale per handle!
	// handle_size *= mpr->scale_final;

	float scale_xyz[3];
	manipulator_calc_rect_view_scale(mpr, dims, scale_xyz);
	margin[0] = ((handle_size * scale_xyz[0]));
	margin[1] = ((handle_size * scale_xyz[1]));
	margin[2] = ((handle_size * scale_xyz[2]));
}

/* -------------------------------------------------------------------- */

static void manipulator_rect_pivot_from_scale_part(int part, float r_pt[3], bool r_constrain_axis[3])
{
	if (part >= ED_MANIPULATOR_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
	    part <= ED_MANIPULATOR_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
	{
		int index = (part - ED_MANIPULATOR_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
		int range[3];
		range[2] = index % 3;
		index    = index / 3;
		range[1] = index % 3;
		index    = index / 3;
		range[0] = index % 3;

		const float sign[3] = {-0.5f, 0.0f, 0.5f};
		for (int i = 0; i < 3; i++) {
			r_pt[i] = 0.5 * sign[range[i]];
			r_constrain_axis[i] = (range[i] == 1);
		}
	}
}

/* -------------------------------------------------------------------- */
/** \name Box Draw Style
 *
 * Useful for 3D views, see: #ED_MANIPULATOR_CAGE2D_STYLE_BOX
 * \{ */

static void cage3d_draw_box_corners(
        const float r[3], const float margin[3], const float color[3])
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
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
	if (highlighted >= ED_MANIPULATOR_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z &&
	    highlighted <= ED_MANIPULATOR_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z)
	{
		int index = (highlighted - ED_MANIPULATOR_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z);
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
			uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
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
 * Useful for 2D views, see: #ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE
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
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	imm_draw_cube_wire_3d(pos, (float[3]){0}, r);

#if 0
	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
		if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
			const float rad[2] = {margin[0] / 2, margin[1] / 2};
			const float center[2] = {0.0f, 0.0f};

			immBegin(GWN_PRIM_LINES, 4);
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
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
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

static void manipulator_cage3d_draw_intern(
        RegionView3D *rv3d,
        wmManipulator *mpr, const bool select, const bool highlight, const int select_id)
{
	// const bool use_clamp = (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) == 0;
	float dims[3];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	float matrix_final[4][4];

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	const int draw_style = RNA_enum_get(mpr->ptr, "draw_style");
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");

	const float size_real[3] = {dims[0] / 2.0f, dims[1] / 2.0f, dims[2] / 2.0f};

	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	float margin[3];
	manipulator_calc_rect_view_margin(mpr, dims, margin);

	/* Handy for quick testing draw (if it's outside bounds). */
	if (false) {
		glEnable(GL_BLEND);
		uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv((const float[4]){1, 1, 1, 0.5f});
		float s = 0.5f;
		immRectf(pos, -s, -s, s, s);
		immUnbindProgram();
		glDisable(GL_BLEND);
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


		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE) {
			for (int i = ED_MANIPULATOR_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
			     i <= ED_MANIPULATOR_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z;
			     i++)
			{
				if (i == ED_MANIPULATOR_CAGE3D_PART_SCALE_MID_X_MID_Y_MID_Z) {
					continue;
				}
				GPU_select_load_id(select_id | i);
				cage3d_draw_box_interaction(
				        mpr->color, i, size, margin);
			}
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
			const int transform_part = ED_MANIPULATOR_CAGE3D_PART_TRANSLATE;
			GPU_select_load_id(select_id | transform_part);
			cage3d_draw_box_interaction(
			        mpr->color, transform_part, size, margin);
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
		if (draw_style == ED_MANIPULATOR_CAGE2D_STYLE_BOX) {
			/* corner manipulators */
			glLineWidth(mpr->line_width + 3.0f);
			cage3d_draw_box_corners(size_real, margin, (const float[3]){0, 0, 0});

			/* corner manipulators */
			float color[4];
			manipulator_color_get(mpr, highlight, color);
			glLineWidth(mpr->line_width);
			cage3d_draw_box_corners(size_real, margin, color);

			bool show = false;
			if (mpr->highlight_part == ED_MANIPULATOR_CAGE3D_PART_TRANSLATE) {
				/* Only show if we're drawing the center handle
				 * otherwise the entire rectangle is the hotspot. */
				if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
					show = true;
				}
			}
			else {
				show = true;
			}

			if (show) {
				cage3d_draw_box_interaction(
				        mpr->color, mpr->highlight_part, size_real, margin);
			}
		}
		else if (draw_style == ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE) {
			float color[4];
			manipulator_color_get(mpr, highlight, color);

			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_POLYGON_SMOOTH);
			glEnable(GL_BLEND);

			glLineWidth(mpr->line_width + 3.0f);
			cage3d_draw_circle_wire(size_real, margin, (const float[3]){0, 0, 0}, transform_flag, draw_options);
			glLineWidth(mpr->line_width);
			cage3d_draw_circle_wire(size_real, margin, color, transform_flag, draw_options);

			/* corner manipulators */
			cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, (const float[3]){0, 0, 0}, true, 60);
			cage3d_draw_circle_handles(rv3d, matrix_final, size_real, margin, color, true, 40);

			glDisable(GL_BLEND);
			glDisable(GL_POLYGON_SMOOTH);
			glDisable(GL_LINE_SMOOTH);
		}
		else {
			BLI_assert(0);
		}
	}

	glLineWidth(1.0);
	gpuPopMatrix();
}

/**
 * For when we want to draw 3d cage in 3d views.
 */
static void manipulator_cage3d_draw_select(const bContext *C, wmManipulator *mpr, int select_id)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	manipulator_cage3d_draw_intern(rv3d, mpr, true, false, select_id);
}

static void manipulator_cage3d_draw(const bContext *C, wmManipulator *mpr)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;
	manipulator_cage3d_draw_intern(rv3d, mpr, false, is_highlight, -1);
}

static int manipulator_cage3d_get_cursor(wmManipulator *mpr)
{
	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		return BC_NSEW_SCROLLCURSOR;
	}

	return CURSOR_STD;
}

typedef struct RectTransformInteraction {
	float orig_mouse[2];
	float orig_matrix_offset[4][4];
	float orig_matrix_final_no_offset[4][4];
	Dial *dial;
} RectTransformInteraction;

static void manipulator_cage3d_setup(wmManipulator *mpr)
{
	mpr->flag |= /* WM_MANIPULATOR_DRAW_MODAL | */ /* TODO */
	             WM_MANIPULATOR_DRAW_NO_SCALE;
}

static int manipulator_cage3d_invoke(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	RectTransformInteraction *data = MEM_callocN(sizeof(RectTransformInteraction), "cage_interaction");

	copy_m4_m4(data->orig_matrix_offset, mpr->matrix_offset);
	manipulator_calc_matrix_final_no_offset(mpr, data->orig_matrix_final_no_offset, true);

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, data->orig_mouse) == 0)
	{
		zero_v2(data->orig_mouse);
	}

	mpr->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

/* XXX. this isn't working properly, for now rely on the modal operators. */
static int manipulator_cage3d_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	/* For transform logic to be managable we operate in -0.5..0.5 2D space,
	 * no matter the size of the rectangle, mouse coorts are scaled to unit space.
	 * The mouse coords have been projected into the matrix so we don't need to worry about axis alignment.
	 *
	 * - The cursor offset are multiplied by 'dims'.
	 * - Matrix translation is also multiplied by 'dims'.
	 */
	RectTransformInteraction *data = mpr->interaction_data;
	float point_local[2];

	float dims[3];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);

	{
		float matrix_back[4][4];
		copy_m4_m4(matrix_back, mpr->matrix_offset);
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);

		bool ok = manipulator_window_project_2d(
		        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, point_local);
		copy_m4_m4(mpr->matrix_offset, matrix_back);
		if (!ok) {
			return OPERATOR_RUNNING_MODAL;
		}
	}

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	wmManipulatorProperty *mpr_prop;

	mpr_prop = WM_manipulator_target_property_find(mpr, "matrix");
	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_get_array(mpr, mpr_prop, &mpr->matrix_offset[0][0]);
	}

	if (mpr->highlight_part == ED_MANIPULATOR_CAGE3D_PART_TRANSLATE) {
		/* do this to prevent clamping from changing size */
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
		mpr->matrix_offset[3][0] = data->orig_matrix_offset[3][0] + (point_local[0] - data->orig_mouse[0]);
		mpr->matrix_offset[3][1] = data->orig_matrix_offset[3][1] + (point_local[1] - data->orig_mouse[1]);
	}
	else if (mpr->highlight_part == ED_MANIPULATOR_CAGE3D_PART_ROTATE) {

#define MUL_V2_V3_M4_FINAL(test_co, mouse_co) \
		mul_v3_m4v3(test_co, data->orig_matrix_final_no_offset, ((const float[3]){UNPACK2(mouse_co), 0.0}))

		float test_co[3];

		if (data->dial == NULL) {
			MUL_V2_V3_M4_FINAL(test_co, data->orig_matrix_offset[3]);

			data->dial = BLI_dial_initialize(test_co, FLT_EPSILON);

			MUL_V2_V3_M4_FINAL(test_co, data->orig_mouse);
			BLI_dial_angle(data->dial, test_co);
		}

		/* rotate */
		MUL_V2_V3_M4_FINAL(test_co, point_local);
		const float angle =  BLI_dial_angle(data->dial, test_co);

		float matrix_space_inv[4][4];
		float matrix_rotate[4][4];
		float pivot[3];

		copy_v3_v3(pivot, data->orig_matrix_offset[3]);

		invert_m4_m4(matrix_space_inv, mpr->matrix_space);

		unit_m4(matrix_rotate);
		mul_m4_m4m4(matrix_rotate, matrix_rotate, matrix_space_inv);
		rotate_m4(matrix_rotate, 'Z', -angle);
		mul_m4_m4m4(matrix_rotate, matrix_rotate, mpr->matrix_space);

		zero_v3(matrix_rotate[3]);
		transform_pivot_set_m4(matrix_rotate, pivot);

		mul_m4_m4m4(mpr->matrix_offset, matrix_rotate, data->orig_matrix_offset);

#undef MUL_V2_V3_M4_FINAL
	}
	else {
		/* scale */
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
		float pivot[3];
		bool constrain_axis[3] = {false};

		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
			manipulator_rect_pivot_from_scale_part(mpr->highlight_part, pivot, constrain_axis);
		}
		else {
			zero_v3(pivot);
		}

		/* Cursor deltas scaled to (-0.5..0.5). */
		float delta_orig[3], delta_curr[3];

		delta_orig[2] = 0.0;
		delta_curr[2] = 0.0;

		for (int i = 0; i < 2; i++) {
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

				if ((transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_SIGNED) == 0) {
					if (sign != signum_i(scale[i])) {
						scale[i] = 0.0f;
					}
				}
			}
		}

		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM) {
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
		mul_m4_m4m4(mpr->matrix_offset, data->orig_matrix_offset, matrix_scale);
	}

	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, &mpr->matrix_offset[0][0]);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(CTX_wm_region(C));
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_cage3d_property_update(wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
{
	if (STREQ(mpr_prop->type->idname, "matrix")) {
		if (WM_manipulator_target_property_array_length(mpr, mpr_prop) == 16) {
			WM_manipulator_target_property_value_get_array(mpr, mpr_prop, &mpr->matrix_offset[0][0]);
		}
		else {
			BLI_assert(0);
		}
	}
	else {
		BLI_assert(0);
	}
}

static void manipulator_cage3d_exit(bContext *C, wmManipulator *mpr, const bool cancel)
{
	RectTransformInteraction *data = mpr->interaction_data;

	MEM_SAFE_FREE(data->dial);

	if (!cancel)
		return;

	wmManipulatorProperty *mpr_prop;

	/* reset properties */
	mpr_prop = WM_manipulator_target_property_find(mpr, "matrix");
	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, &data->orig_matrix_offset[0][0]);
	}

	copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
}


/* -------------------------------------------------------------------- */
/** \name Cage Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_cage_3d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_cage_3d";

	/* api callbacks */
	wt->draw = manipulator_cage3d_draw;
	wt->draw_select = manipulator_cage3d_draw_select;
	wt->setup = manipulator_cage3d_setup;
	wt->invoke = manipulator_cage3d_invoke;
	wt->property_update = manipulator_cage3d_property_update;
	wt->modal = manipulator_cage3d_modal;
	wt->exit = manipulator_cage3d_exit;
	wt->cursor_get = manipulator_cage3d_get_cursor;

	wt->struct_size = sizeof(wmManipulator);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_MANIPULATOR_CAGE2D_STYLE_BOX, "BOX", 0, "Box", ""},
		{ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE, "CIRCLE", 0, "Circle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_transform[] = {
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE, "SCALE", 0, "Scale", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM, "SCALE_UNIFORM", 0, "Scale Uniform", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "XFORM_CENTER_HANDLE", 0, "Center Handle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static float unit_v3[3] = {1.0f, 1.0f, 1.0f};
	RNA_def_float_vector(wt->srna, "dimensions", 3, unit_v3, 0, FLT_MAX, "Dimensions", "", 0.0f, FLT_MAX);
	RNA_def_enum_flag(wt->srna, "transform", rna_enum_transform, 0, "Transform Options", "");
	RNA_def_enum(wt->srna, "draw_style", rna_enum_draw_style, ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE, "Draw Style", "");
	RNA_def_enum_flag(
	        wt->srna, "draw_options", rna_enum_draw_options,
	        ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "Draw Options", "");

	WM_manipulatortype_target_property_def(wt, "matrix", PROP_FLOAT, 16);
}

void ED_manipulatortypes_cage_3d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_cage_3d);
}

/** \} */
