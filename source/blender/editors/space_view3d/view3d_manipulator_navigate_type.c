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

/** \file view3d_manipulator_navigate_type.c
 *  \ingroup wm
 *
 * \name Custom Orientation/Navigation Manipulator for the 3D View
 *
 * \brief Simple manipulator to axis and translate.
 *
 * - scale_basis: used for the size.
 * - matrix_basis: used for the location.
 * - matrix_offset: used to store the orientation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_context.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "view3d_intern.h"

#define DIAL_RESOLUTION 32

#define HANDLE_SIZE 0.33

static void axis_geom_draw(
        const wmManipulator *mpr, const float color[4], const bool UNUSED(select))
{
	glLineWidth(mpr->line_width);

	Gwn_VertFormat *format = immVertexFormat();
	const uint pos_id = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	/* flip z for reverse */
	const float cone_coords[5][3] = {
		{-1, -1, 4},
		{-1, +1, 4},
		{+1, +1, 4},
		{+1, -1, 4},
		{0,   0, 2},
	};

	struct {
		float depth;
		char index;
		char axis;
		char is_pos;
	} axis_order[6] = {
		{-mpr->matrix_offset[0][2], 0, 0, false},
		{+mpr->matrix_offset[0][2], 1, 0, true},
		{-mpr->matrix_offset[1][2], 2, 1, false},
		{+mpr->matrix_offset[1][2], 3, 1, true},
		{-mpr->matrix_offset[2][2], 4, 2, false},
		{+mpr->matrix_offset[2][2], 5, 2, true},
	};
	qsort(&axis_order, ARRAY_SIZE(axis_order), sizeof(axis_order[0]), BLI_sortutil_cmp_float);

	const float scale_axis = 0.25f;
	static const float axis_highlight[4] = {1, 1, 1, 1};
	static const float axis_nop[4] = {1, 1, 1, 0};
	static const float axis_black[4] = {0, 0, 0, 1};
	static float axis_color[3][4];
	gpuPushMatrix();
	gpuMultMatrix(mpr->matrix_offset);

	bool draw_center_done = false;

	for (int axis_index = 0; axis_index < ARRAY_SIZE(axis_order); axis_index++) {
		const int index = axis_order[axis_index].index;
		const int axis = axis_order[axis_index].axis;
		const bool is_pos = axis_order[axis_index].is_pos;

		/* Draw slightly before, so axis aligned arrows draw ontop. */
		if ((draw_center_done == false) && (axis_order[axis_index].depth > -0.01f)) {

			/* Circle defining active area (revert back to 2D space). */
			{
				gpuPopMatrix();
				immUniformColor4fv(color);
				imm_draw_circle_fill_3d(pos_id, 0, 0, 1.0f, DIAL_RESOLUTION);
				gpuPushMatrix();
				gpuMultMatrix(mpr->matrix_offset);
			}

			/* Center cube. */
			{
				float center[3], size[3];

				zero_v3(center);
				copy_v3_fl(size, HANDLE_SIZE);

				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LEQUAL);
				glBlendFunc(GL_ONE, GL_ZERO);
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

				glEnable(GL_LINE_SMOOTH);
				glEnable(GL_BLEND);
				glLineWidth(1.0f);
				/* Just draw depth values. */
				immUniformColor4fv(axis_nop);
				imm_draw_cube_fill_3d(pos_id, center, size);
				immUniformColor4fv(axis_black);
				madd_v3_v3fl(
				        center,
				        (float[3]){
				            mpr->matrix_offset[0][2],
				            mpr->matrix_offset[1][2],
				            mpr->matrix_offset[2][2],
				        },
				        0.08f);
				imm_draw_cube_wire_3d(pos_id, center, size);
				glDisable(GL_BLEND);
				glDisable(GL_LINE_SMOOTH);
				glDisable(GL_DEPTH_TEST);
			}

			draw_center_done = true;
		}
		UI_GetThemeColor3fv(TH_AXIS_X + axis, axis_color[axis]);
		axis_color[axis][3] = 1.0f;

		const int index_z = axis;
		const int index_y = (axis + 1) % 3;
		const int index_x = (axis + 2) % 3;

#define ROTATED_VERT(v_orig) \
		{ \
			float v[3]; \
			copy_v3_v3(v, v_orig); \
			if (is_pos == 0) { \
				v[2] *= -1.0f; \
			} \
			immVertex3f(pos_id, v[index_x] * scale_axis, v[index_y] * scale_axis, v[index_z] * scale_axis); \
		} ((void)0)

		bool ok = true;

		/* skip view align axis */
		if (len_squared_v2(mpr->matrix_offset[axis]) < 1e-6f && (mpr->matrix_offset[axis][2] > 0.0f) == is_pos) {
			ok = false;
		}
		if (ok) {
			immUniformColor4fv(index + 1 == mpr->highlight_part ? axis_highlight : axis_color[axis]);
			immBegin(GWN_PRIM_TRI_FAN, 6);
			ROTATED_VERT(cone_coords[4]);
			for (int j = 0; j <= 4; j++) {
				ROTATED_VERT(cone_coords[j % 4]);
			}
			immEnd();
		}

#undef ROTATED_VERT
	}

	gpuPopMatrix();
	immUnbindProgram();
}

static void axis3d_draw_intern(
        const bContext *UNUSED(C), wmManipulator *mpr,
        const bool select, const bool highlight)
{
	const float *color = highlight ? mpr->color_hi : mpr->color;
	float matrix_final[4][4];
	float matrix_unit[4][4];

	unit_m4(matrix_unit);

	WM_manipulator_calc_matrix_final_params(
	        mpr,
	        &((struct WM_ManipulatorMatrixParams) {
	            .matrix_offset = matrix_unit,
	        }), matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	glEnable(GL_BLEND);
	axis_geom_draw(mpr, color, select);
	glDisable(GL_BLEND);
	gpuPopMatrix();
}

static void manipulator_axis_draw(const bContext *C, wmManipulator *mpr)
{
	const bool is_modal = mpr->state & WM_MANIPULATOR_STATE_MODAL;
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	(void)is_modal;

	glEnable(GL_BLEND);
	axis3d_draw_intern(C, mpr, false, is_highlight);
	glDisable(GL_BLEND);
}

static int manipulator_axis_test_select(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	float point_local[2] = {UNPACK2(event->mval)};
	sub_v2_v2(point_local, mpr->matrix_basis[3]);
	mul_v2_fl(point_local, 1.0f / (mpr->scale_basis * UI_DPI_FAC));

	const float len_sq = len_squared_v2(point_local);
	if (len_sq > 1.0) {
		return -1;
	}

	int part_best = -1;
	int part_index = 1;
	/* Use 'SQUARE(HANDLE_SIZE)' if we want to be able to _not_ focus on one of the axis. */
	float i_best_len_sq = FLT_MAX;
	for (int i = 0; i < 3; i++) {
		for (int is_pos = 0; is_pos < 2; is_pos++) {
			float co[2] = {
				mpr->matrix_offset[i][0] * (is_pos ? 1 : -1),
				mpr->matrix_offset[i][1] * (is_pos ? 1 : -1),
			};

			bool ok = true;

			/* Check if we're viewing on an axis, there is no point to clicking on the current axis so show the reverse. */
			if (len_squared_v2(co) < 1e-6f && (mpr->matrix_offset[i][2] > 0.0f) == is_pos) {
				ok = false;
			}

			if (ok) {
				const float len_axis_sq = len_squared_v2v2(co, point_local);
				if (len_axis_sq < i_best_len_sq) {
					part_best = part_index;
					i_best_len_sq = len_axis_sq;
				}
			}
			part_index += 1;
		}
	}

	if (part_best != -1) {
		return part_best;
	}

	/* The 'mpr->scale_final' is already applied when projecting. */
	if (len_sq < 1.0f) {
		return 0;
	}

	return -1;
}

static int manipulator_axis_cursor_get(wmManipulator *mpr)
{
	if (mpr->highlight_part > 0) {
		return CURSOR_EDIT;
	}
	return BC_NSEW_SCROLLCURSOR;
}

void VIEW3D_WT_navigate_rotate(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "VIEW3D_WT_navigate_rotate";

	/* api callbacks */
	wt->draw = manipulator_axis_draw;
	wt->test_select = manipulator_axis_test_select;
	wt->cursor_get = manipulator_axis_cursor_get;

	wt->struct_size = sizeof(wmManipulator);
}
