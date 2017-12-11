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

/** \file grab3d_manipulator.c
 *  \ingroup wm
 *
 * \name Grab Manipulator
 *
 * 3D Manipulator, also works in 2D views.
 *
 * \brief Simple manipulator to grab and translate.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` currently not used.
 * - `matrix[2]` is the widget direction (for all manipulators).
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

/* own includes */
#include "../manipulator_geometry.h"
#include "../manipulator_library_intern.h"

typedef struct GrabManipulator3D {
	wmManipulator manipulator;
	/* Added to 'matrix_basis' when calculating the matrix. */
	float prop_co[3];
} GrabManipulator3D;

static void manipulator_grab_matrix_basis_get(const wmManipulator *mpr, float r_matrix[4][4])
{
	GrabManipulator3D *grab = (GrabManipulator3D *)mpr;

	copy_m4_m4(r_matrix, grab->manipulator.matrix_basis);
	add_v3_v3(r_matrix[3], grab->prop_co);
}

static int manipulator_grab_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak tweak_flag);

typedef struct GrabInteraction {
	float init_mval[2];

	/* only for when using properties */
	float init_prop_co[3];

	float init_matrix_final[4][4];
} GrabInteraction;

#define DIAL_RESOLUTION 32

/* -------------------------------------------------------------------- */

static void grab_geom_draw(
        const wmManipulator *mpr, const float color[4], const bool select, const int draw_options)
{
#ifdef USE_MANIPULATOR_CUSTOM_DIAL
	UNUSED_VARS(grab3d, col, axis_modal_mat);
	wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_grab3d, select);
#else
	const int draw_style = RNA_enum_get(mpr->ptr, "draw_style");
	const bool filled = (draw_options & ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL) != 0;

	glLineWidth(mpr->line_width);

	Gwn_VertFormat *format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	if (draw_style == ED_MANIPULATOR_GRAB_STYLE_RING_2D) {
		if (filled) {
			imm_draw_circle_fill_2d(pos, 0, 0, 1.0f, DIAL_RESOLUTION);
		}
		else {
			imm_draw_circle_wire_2d(pos, 0, 0, 1.0f, DIAL_RESOLUTION);
		}
	}
	else if (draw_style == ED_MANIPULATOR_GRAB_STYLE_CROSS_2D) {
		immBegin(GWN_PRIM_LINES, 4);
		immVertex2f(pos,  1.0f,  1.0f);
		immVertex2f(pos, -1.0f, -1.0f);

		immVertex2f(pos, -1.0f,  1.0f);
		immVertex2f(pos,  1.0f, -1.0f);
		immEnd();
	}
	else {
		BLI_assert(0);
	}

	immUnbindProgram();

	UNUSED_VARS(select);
#endif
}

static void grab3d_get_translate(
        const wmManipulator *mpr, const wmEvent *event, const ARegion *ar,
        float co_delta[3])
{
	GrabInteraction *inter = mpr->interaction_data;
	const float mval_delta[2] = {
	    event->mval[0] - inter->init_mval[0],
	    event->mval[1] - inter->init_mval[1],
	};

	RegionView3D *rv3d = ar->regiondata;
	float co_ref[3];
	mul_v3_mat3_m4v3(co_ref, mpr->matrix_space, inter->init_prop_co);
	const float zfac = ED_view3d_calc_zfac(rv3d, co_ref, NULL);

	ED_view3d_win_to_delta(ar, mval_delta, co_delta, zfac);

	float matrix_space_inv[3][3];
	copy_m3_m4(matrix_space_inv, mpr->matrix_space);
	invert_m3(matrix_space_inv);
	mul_m3_v3(matrix_space_inv, co_delta);
}

static void grab3d_draw_intern(
        const bContext *C, wmManipulator *mpr,
        const bool select, const bool highlight)
{
	GrabInteraction *inter = mpr->interaction_data;
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
	const bool align_view = (draw_options & ED_MANIPULATOR_GRAB_DRAW_FLAG_ALIGN_VIEW) != 0;
	float color[4];
	float matrix_final[4][4];
	float matrix_align[4][4];

	manipulator_color_get(mpr, highlight, color);
	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	if (align_view) {
		float matrix_final_unit[4][4];
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		normalize_m4_m4(matrix_final_unit, matrix_final);
		mul_m4_m4m4(matrix_align, rv3d->viewmat, matrix_final_unit);
		zero_v3(matrix_align[3]);
		transpose_m4(matrix_align);
		gpuMultMatrix(matrix_align);
	}

	glEnable(GL_BLEND);
	grab_geom_draw(mpr, color, select, draw_options);
	glDisable(GL_BLEND);
	gpuPopMatrix();

	if (mpr->interaction_data) {
		gpuPushMatrix();
		gpuMultMatrix(inter->init_matrix_final);

		if (align_view) {
			gpuMultMatrix(matrix_align);
		}

		glEnable(GL_BLEND);
		grab_geom_draw(mpr, (const float [4]){0.5f, 0.5f, 0.5f, 0.5f}, select, draw_options);
		glDisable(GL_BLEND);
		gpuPopMatrix();
	}
}

static void manipulator_grab_draw_select(const bContext *C, wmManipulator *mpr, int select_id)
{
	GPU_select_load_id(select_id);
	grab3d_draw_intern(C, mpr, true, false);
}

static void manipulator_grab_draw(const bContext *C, wmManipulator *mpr)
{
	const bool is_modal = mpr->state & WM_MANIPULATOR_STATE_MODAL;
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	(void)is_modal;

	glEnable(GL_BLEND);
	grab3d_draw_intern(C, mpr, false, is_highlight);
	glDisable(GL_BLEND);
}

static int manipulator_grab_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	GrabManipulator3D *grab = (GrabManipulator3D *)mpr;
	GrabInteraction *inter = mpr->interaction_data;
	ARegion *ar = CTX_wm_region(C);

	float prop_delta[3];
	if (CTX_wm_area(C)->spacetype == SPACE_VIEW3D) {
		grab3d_get_translate(mpr, event, ar, prop_delta);
	}
	else {
		float mval_proj_init[2], mval_proj_curr[2];
		if ((manipulator_window_project_2d(
		         C, mpr, inter->init_mval, 2, false, mval_proj_init) == false) ||
		    (manipulator_window_project_2d(
		         C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, mval_proj_curr) == false))
		{
			return OPERATOR_RUNNING_MODAL;
		}
		sub_v2_v2v2(prop_delta, mval_proj_curr, mval_proj_init);
		prop_delta[2] = 0.0f;
	}
	add_v3_v3v3(grab->prop_co, inter->init_prop_co, prop_delta);

	/* set the property for the operator and call its modal function */
	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, grab->prop_co);
	}
	else {
		zero_v3(grab->prop_co);
	}

	ED_region_tag_redraw(ar);

	return OPERATOR_RUNNING_MODAL;
}

static int manipulator_grab_invoke(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	GrabInteraction *inter = MEM_callocN(sizeof(GrabInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

#if 0
	copy_v3_v3(inter->init_prop_co, grab->prop_co);
#else
	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		WM_manipulator_target_property_value_get_array(mpr, mpr_prop, inter->init_prop_co);
	}
#endif

	WM_manipulator_calc_matrix_final(mpr, inter->init_matrix_final);

	mpr->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}


static int manipulator_grab_test_select(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	float point_local[2];

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, true, point_local) == false)
	{
		return -1;
	}

	/* The 'mpr->scale_final' is already applied when projecting. */
	if (len_squared_v2(point_local) < 1.0f) {
		return 0;
	}

	return -1;
}

static void manipulator_grab_property_update(wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
{
	GrabManipulator3D *grab = (GrabManipulator3D *)mpr;
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		WM_manipulator_target_property_value_get_array(mpr, mpr_prop, grab->prop_co);
	}
	else {
		zero_v3(grab->prop_co);
	}
}

static int manipulator_grab_cursor_get(wmManipulator *UNUSED(mpr))
{
	return BC_HANDCURSOR;
}

/* -------------------------------------------------------------------- */
/** \name Grab Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_grab_3d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_grab_3d";

	/* api callbacks */
	wt->draw = manipulator_grab_draw;
	wt->draw_select = manipulator_grab_draw_select;
	wt->test_select = manipulator_grab_test_select;
	wt->matrix_basis_get = manipulator_grab_matrix_basis_get;
	wt->invoke = manipulator_grab_invoke;
	wt->property_update = manipulator_grab_property_update;
	wt->modal = manipulator_grab_modal;
	wt->cursor_get = manipulator_grab_cursor_get;

	wt->struct_size = sizeof(GrabManipulator3D);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_MANIPULATOR_GRAB_STYLE_RING_2D, "RING_2D", 0, "Ring", ""},
		{ED_MANIPULATOR_GRAB_STYLE_CROSS_2D, "CROSS_2D", 0, "Ring", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL, "FILL", 0, "Filled", ""},
		{ED_MANIPULATOR_GRAB_DRAW_FLAG_ALIGN_VIEW, "ALIGN_VIEW", 0, "Align View", ""},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_enum(wt->srna, "draw_style", rna_enum_draw_style, ED_MANIPULATOR_GRAB_STYLE_RING_2D, "Draw Style", "");
	RNA_def_enum_flag(wt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	WM_manipulatortype_target_property_def(wt, "offset", PROP_FLOAT, 3);
}

void ED_manipulatortypes_grab_3d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_grab_3d);
}

/** \} */ // Grab Manipulator API
