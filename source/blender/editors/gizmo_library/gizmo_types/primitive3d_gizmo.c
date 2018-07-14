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

/** \file primitive3d_gizmo.c
 *  \ingroup wm
 *
 * \name Primitive Gizmo
 *
 * 3D Gizmo
 *
 * \brief Gizmo with primitive drawing type (plane, cube, etc.).
 * Currently only plane primitive supported without own handling, use with operator only.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gizmo_library.h"

/* own includes */
#include "../gizmo_library_intern.h"

static float verts_plane[4][3] = {
	{-1, -1, 0},
	{ 1, -1, 0},
	{ 1,  1, 0},
	{-1,  1, 0},
};


/* -------------------------------------------------------------------- */

static void gizmo_primitive_draw_geom(
        const float col_inner[4], const float col_outer[4], const int draw_style)
{
	float (*verts)[3];
	uint vert_count = 0;

	if (draw_style == ED_GIZMO_PRIMITIVE_STYLE_PLANE) {
		verts = verts_plane;
		vert_count = ARRAY_SIZE(verts_plane);
	}

	if (vert_count > 0) {
		uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		wm_gizmo_vec_draw(col_inner, verts, vert_count, pos, GWN_PRIM_TRI_FAN);
		wm_gizmo_vec_draw(col_outer, verts, vert_count, pos, GWN_PRIM_LINE_LOOP);
		immUnbindProgram();
	}
}

static void gizmo_primitive_draw_intern(
        wmGizmo *mpr, const bool UNUSED(select),
        const bool highlight)
{
	float color_inner[4], color_outer[4];
	float matrix_final[4][4];
	const int draw_style = RNA_enum_get(mpr->ptr, "draw_style");

	gizmo_color_get(mpr, highlight, color_outer);
	copy_v4_v4(color_inner, color_outer);
	color_inner[3] *= 0.5f;

	WM_gizmo_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	GPU_blend(true);
	gizmo_primitive_draw_geom(color_inner, color_outer, draw_style);
	GPU_blend(false);

	gpuPopMatrix();

	if (mpr->interaction_data) {
		GizmoInteraction *inter = mpr->interaction_data;

		copy_v4_fl(color_inner, 0.5f);
		copy_v3_fl(color_outer, 0.5f);
		color_outer[3] = 0.8f;

		gpuPushMatrix();
		gpuMultMatrix(inter->init_matrix_final);

		GPU_blend(true);
		gizmo_primitive_draw_geom(color_inner, color_outer, draw_style);
		GPU_blend(false);

		gpuPopMatrix();
	}
}

static void gizmo_primitive_draw_select(
        const bContext *UNUSED(C), wmGizmo *mpr,
        int select_id)
{
	GPU_select_load_id(select_id);
	gizmo_primitive_draw_intern(mpr, true, false);
}

static void gizmo_primitive_draw(const bContext *UNUSED(C), wmGizmo *mpr)
{
	gizmo_primitive_draw_intern(
	        mpr, false,
	        (mpr->state & WM_GIZMO_STATE_HIGHLIGHT));
}

static void gizmo_primitive_setup(wmGizmo *mpr)
{
	mpr->flag |= WM_GIZMO_DRAW_MODAL;
}

static int gizmo_primitive_invoke(
        bContext *UNUSED(C), wmGizmo *mpr, const wmEvent *UNUSED(event))
{
	GizmoInteraction *inter = MEM_callocN(sizeof(GizmoInteraction), __func__);

	WM_gizmo_calc_matrix_final(mpr, inter->init_matrix_final);

	mpr->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

/* -------------------------------------------------------------------- */
/** \name Primitive Gizmo API
 *
 * \{ */

static void GIZMO_WT_primitive_3d(wmGizmoType *wt)
{
	/* identifiers */
	wt->idname = "GIZMO_WT_primitive_3d";

	/* api callbacks */
	wt->draw = gizmo_primitive_draw;
	wt->draw_select = gizmo_primitive_draw_select;
	wt->setup = gizmo_primitive_setup;
	wt->invoke = gizmo_primitive_invoke;

	wt->struct_size = sizeof(wmGizmo);

	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_GIZMO_PRIMITIVE_STYLE_PLANE, "PLANE", 0, "Plane", ""},
		{0, NULL, 0, NULL, NULL}
	};
	RNA_def_enum(wt->srna, "draw_style", rna_enum_draw_style, ED_GIZMO_PRIMITIVE_STYLE_PLANE, "Draw Style", "");
}

void ED_gizmotypes_primitive_3d(void)
{
	WM_gizmotype_append(GIZMO_WT_primitive_3d);
}

/** \} */
