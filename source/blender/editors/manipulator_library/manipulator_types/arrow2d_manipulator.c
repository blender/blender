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

/** \file arrow2d_manipulator.c
 *  \ingroup wm
 *
 * \name 2D Arrow Manipulator
 *
 * \brief Simple arrow manipulator which is dragged into a certain direction.
 */

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_windowmanager_types.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "ED_screen.h"
#include "ED_manipulator_library.h"

/* own includes */
#include "WM_api.h"

#include "../manipulator_library_intern.h"

static void arrow2d_draw_geom(wmManipulator *mpr, const float matrix[4][4], const float color[4])
{
	const float size = 0.11f;
	const float size_breadth = size / 2.0f;
	const float size_length = size * 1.7f;
	/* Subtract the length so the arrow fits in the hotspot. */
	const float arrow_length = RNA_float_get(mpr->ptr, "length") - size_length;
	const float arrow_angle = RNA_float_get(mpr->ptr, "angle");

	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	gpuPushMatrix();
	gpuMultMatrix(matrix);
	gpuRotate2D(RAD2DEGF(arrow_angle));

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GWN_PRIM_LINES, 2);
	immVertex2f(pos, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, arrow_length);
	immEnd();

	immBegin(GWN_PRIM_TRIS, 3);
	immVertex2f(pos, size_breadth, arrow_length);
	immVertex2f(pos, -size_breadth, arrow_length);
	immVertex2f(pos, 0.0f, arrow_length + size_length);
	immEnd();

	immUnbindProgram();

	gpuPopMatrix();
}

static void manipulator_arrow2d_draw(const bContext *UNUSED(C), wmManipulator *mpr)
{
	float color[4];

	float matrix_final[4][4];

	manipulator_color_get(mpr, mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT, color);

	glLineWidth(mpr->line_width);

	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	glEnable(GL_BLEND);
	arrow2d_draw_geom(mpr, matrix_final, color);
	glDisable(GL_BLEND);

	if (mpr->interaction_data) {
		ManipulatorInteraction *inter = mpr->interaction_data;

		glEnable(GL_BLEND);
		arrow2d_draw_geom(mpr, inter->init_matrix_final, (const float[4]){0.5f, 0.5f, 0.5f, 0.5f});
		glDisable(GL_BLEND);
	}
}

static void manipulator_arrow2d_setup(wmManipulator *mpr)
{
	mpr->flag |= WM_MANIPULATOR_DRAW_MODAL;
}

static int manipulator_arrow2d_invoke(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *UNUSED(event))
{
	ManipulatorInteraction *inter = MEM_callocN(sizeof(ManipulatorInteraction), __func__);

	copy_m4_m4(inter->init_matrix_basis, mpr->matrix_basis);
	WM_manipulator_calc_matrix_final(mpr, inter->init_matrix_final);

	mpr->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static int manipulator_arrow2d_test_select(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	const float mval[2] = {event->mval[0], event->mval[1]};
	const float arrow_length = RNA_float_get(mpr->ptr, "length");
	const float arrow_angle = RNA_float_get(mpr->ptr, "angle");
	const float line_len = arrow_length * mpr->scale_final;
	float mval_local[2];

	copy_v2_v2(mval_local, mval);
	sub_v2_v2(mval_local, mpr->matrix_basis[3]);

	float line[2][2];
	line[0][0] = line[0][1] = line[1][0] = 0.0f;
	line[1][1] = line_len;

	/* rotate only if needed */
	if (arrow_angle != 0.0f) {
		float rot_point[2];
		copy_v2_v2(rot_point, line[1]);
		rotate_v2_v2fl(line[1], rot_point, arrow_angle);
	}

	/* arrow line intersection check */
	float isect_1[2], isect_2[2];
	const int isect = isect_line_sphere_v2(
	        line[0], line[1], mval_local, MANIPULATOR_HOTSPOT + mpr->line_width * 0.5f,
	        isect_1, isect_2);

	if (isect > 0) {
		float line_ext[2][2]; /* extended line for segment check including hotspot */
		copy_v2_v2(line_ext[0], line[0]);
		line_ext[1][0] = line[1][0] + MANIPULATOR_HOTSPOT * ((line[1][0] - line[0][0]) / line_len);
		line_ext[1][1] = line[1][1] + MANIPULATOR_HOTSPOT * ((line[1][1] - line[0][1]) / line_len);

		const float lambda_1 = line_point_factor_v2(isect_1, line_ext[0], line_ext[1]);
		if (isect == 1) {
			if (IN_RANGE_INCL(lambda_1, 0.0f, 1.0f)) {
				return 0;
			}
		}
		else {
			BLI_assert(isect == 2);
			const float lambda_2 = line_point_factor_v2(isect_2, line_ext[0], line_ext[1]);
			if (IN_RANGE_INCL(lambda_1, 0.0f, 1.0f) && IN_RANGE_INCL(lambda_2, 0.0f, 1.0f)) {
				return 0;
			}
		}
	}

	return -1;
}

/* -------------------------------------------------------------------- */
/** \name 2D Arrow Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_arrow_2d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_arrow_2d";

	/* api callbacks */
	wt->draw = manipulator_arrow2d_draw;
	wt->setup = manipulator_arrow2d_setup;
	wt->invoke = manipulator_arrow2d_invoke;
	wt->test_select = manipulator_arrow2d_test_select;

	wt->struct_size = sizeof(wmManipulator);

	/* rna */
	RNA_def_float(wt->srna, "length", 1.0f, 0.0f, FLT_MAX, "Arrow Line Length", "", 0.0f, FLT_MAX);
	RNA_def_float_rotation(
	        wt->srna, "angle", 0, NULL, DEG2RADF(-360.0f), DEG2RADF(360.0f),
	        "Roll", "", DEG2RADF(-360.0f), DEG2RADF(360.0f));
}

void ED_manipulatortypes_arrow_2d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_arrow_2d);
}

/** \} */
