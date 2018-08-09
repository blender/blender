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

/** \file arrow2d_gizmo.c
 *  \ingroup wm
 *
 * \name 2D Arrow Gizmo
 *
 * \brief Simple arrow gizmo which is dragged into a certain direction.
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
#include "GPU_state.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

/* own includes */
#include "WM_api.h"

#include "../gizmo_library_intern.h"

static void arrow2d_draw_geom(wmGizmo *gz, const float matrix[4][4], const float color[4])
{
	const float size = 0.11f;
	const float size_breadth = size / 2.0f;
	const float size_length = size * 1.7f;
	/* Subtract the length so the arrow fits in the hotspot. */
	const float arrow_length = RNA_float_get(gz->ptr, "length") - size_length;
	const float arrow_angle = RNA_float_get(gz->ptr, "angle");

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	GPU_matrix_push();
	GPU_matrix_mul(matrix);
	GPU_matrix_rotate_2d(RAD2DEGF(arrow_angle));

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GPU_PRIM_LINES, 2);
	immVertex2f(pos, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, arrow_length);
	immEnd();

	immBegin(GPU_PRIM_TRIS, 3);
	immVertex2f(pos, size_breadth, arrow_length);
	immVertex2f(pos, -size_breadth, arrow_length);
	immVertex2f(pos, 0.0f, arrow_length + size_length);
	immEnd();

	immUnbindProgram();

	GPU_matrix_pop();
}

static void gizmo_arrow2d_draw(const bContext *UNUSED(C), wmGizmo *gz)
{
	float color[4];

	float matrix_final[4][4];

	gizmo_color_get(gz, gz->state & WM_GIZMO_STATE_HIGHLIGHT, color);

	GPU_line_width(gz->line_width);

	WM_gizmo_calc_matrix_final(gz, matrix_final);

	GPU_blend(true);
	arrow2d_draw_geom(gz, matrix_final, color);
	GPU_blend(false);

	if (gz->interaction_data) {
		GizmoInteraction *inter = gz->interaction_data;

		GPU_blend(true);
		arrow2d_draw_geom(gz, inter->init_matrix_final, (const float[4]){0.5f, 0.5f, 0.5f, 0.5f});
		GPU_blend(false);
	}
}

static void gizmo_arrow2d_setup(wmGizmo *gz)
{
	gz->flag |= WM_GIZMO_DRAW_MODAL;
}

static int gizmo_arrow2d_invoke(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *UNUSED(event))
{
	GizmoInteraction *inter = MEM_callocN(sizeof(GizmoInteraction), __func__);

	copy_m4_m4(inter->init_matrix_basis, gz->matrix_basis);
	WM_gizmo_calc_matrix_final(gz, inter->init_matrix_final);

	gz->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static int gizmo_arrow2d_test_select(
        bContext *UNUSED(C), wmGizmo *gz, const int mval[2])
{
	const float mval_fl[2] = {UNPACK2(mval)};
	const float arrow_length = RNA_float_get(gz->ptr, "length");
	const float arrow_angle = RNA_float_get(gz->ptr, "angle");
	const float line_len = arrow_length * gz->scale_final;
	float mval_local[2];

	copy_v2_v2(mval_local, mval_fl);
	sub_v2_v2(mval_local, gz->matrix_basis[3]);

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
	        line[0], line[1], mval_local, GIZMO_HOTSPOT + gz->line_width * 0.5f,
	        isect_1, isect_2);

	if (isect > 0) {
		float line_ext[2][2]; /* extended line for segment check including hotspot */
		copy_v2_v2(line_ext[0], line[0]);
		line_ext[1][0] = line[1][0] + GIZMO_HOTSPOT * ((line[1][0] - line[0][0]) / line_len);
		line_ext[1][1] = line[1][1] + GIZMO_HOTSPOT * ((line[1][1] - line[0][1]) / line_len);

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
/** \name 2D Arrow Gizmo API
 *
 * \{ */

static void GIZMO_GT_arrow_2d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_arrow_2d";

	/* api callbacks */
	gzt->draw = gizmo_arrow2d_draw;
	gzt->setup = gizmo_arrow2d_setup;
	gzt->invoke = gizmo_arrow2d_invoke;
	gzt->test_select = gizmo_arrow2d_test_select;

	gzt->struct_size = sizeof(wmGizmo);

	/* rna */
	RNA_def_float(gzt->srna, "length", 1.0f, 0.0f, FLT_MAX, "Arrow Line Length", "", 0.0f, FLT_MAX);
	RNA_def_float_rotation(
	        gzt->srna, "angle", 0, NULL, DEG2RADF(-360.0f), DEG2RADF(360.0f),
	        "Roll", "", DEG2RADF(-360.0f), DEG2RADF(360.0f));
}

void ED_gizmotypes_arrow_2d(void)
{
	WM_gizmotype_append(GIZMO_GT_arrow_2d);
}

/** \} */
