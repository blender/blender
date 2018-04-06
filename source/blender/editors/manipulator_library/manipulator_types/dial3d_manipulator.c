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

/** \file dial3d_manipulator.c
 *  \ingroup wm
 *
 * \name Dial Manipulator
 *
 * 3D Manipulator
 *
 * \brief Circle shaped manipulator for circular interaction.
 * Currently no own handling, use with operator only.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' when DialManipulator.use_start_y_axis is set.
 * - `matrix[2]` is the axis the dial rotates around (all dials).
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

/* to use custom dials exported to geom_dial_manipulator.c */
// #define USE_MANIPULATOR_CUSTOM_DIAL

static int manipulator_dial_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak tweak_flag);

typedef struct DialInteraction {
	float init_mval[2];

	/* only for when using properties */
	float init_prop_angle;

	/* cache the last angle to detect rotations bigger than -/+ PI */
	float last_angle;
	/* number of full rotations */
	int rotations;

	/* final output values, used for drawing */
	struct {
		float angle_ofs;
		float angle_delta;
	} output;
} DialInteraction;

#define DIAL_WIDTH       1.0f
#define DIAL_RESOLUTION 32

/**
 * We can't use this for the #wmManipulatorType.matrix_basis_get callback, it conflicts with depth picking.
 */
static void dial_calc_matrix(const wmManipulator *mpr, float mat[4][4])
{
	float rot[3][3];
	const float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, mpr->matrix_basis[2]);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], mpr->matrix_basis[3]);
}

/* -------------------------------------------------------------------- */

static void dial_geom_draw(
        const wmManipulator *mpr, const float color[4], const bool select,
        float axis_modal_mat[4][4], float clip_plane[4])
{
#ifdef USE_MANIPULATOR_CUSTOM_DIAL
	UNUSED_VARS(dial, col, axis_modal_mat, clip_plane);
	wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_dial, select);
#else
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
	const bool filled = (draw_options & ED_MANIPULATOR_DIAL_DRAW_FLAG_FILL) != 0;

	glLineWidth(mpr->line_width);

	Gwn_VertFormat *format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	if (clip_plane) {
		immBindBuiltinProgram(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR);
		float clip_plane_f[4] = {clip_plane[0], clip_plane[1], clip_plane[2], clip_plane[3]};
		immUniform4fv("ClipPlane", clip_plane_f);
		immUniformMatrix4fv("ModelMatrix", axis_modal_mat);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	immUniformColor4fv(color);

	if (filled) {
		imm_draw_circle_fill_2d(pos, 0, 0, 1.0, DIAL_RESOLUTION);
	}
	else {
		imm_draw_circle_wire_2d(pos, 0, 0, 1.0, DIAL_RESOLUTION);
	}

	immUnbindProgram();

	UNUSED_VARS(select);
#endif
}

/**
 * Draws a line from (0, 0, 0) to \a co_outer, at \a angle.
 */
static void dial_ghostarc_draw_helpline(const float angle, const float co_outer[3], const float color[4])
{
	glLineWidth(1.0f);

	gpuPushMatrix();
	gpuRotate3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GWN_PRIM_LINE_STRIP, 2);
	immVertex3f(pos, 0.0f, 0.0f, 0.0f);
	immVertex3fv(pos, co_outer);
	immEnd();

	immUnbindProgram();

	gpuPopMatrix();
}

static void dial_ghostarc_draw(
        const wmManipulator *mpr, const float angle_ofs, const float angle_delta, const float color[4])
{
	const float width_inner = DIAL_WIDTH - mpr->line_width * 0.5f / U.manipulator_size;

	Gwn_VertFormat *format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	imm_draw_disk_partial_fill_2d(
	        pos, 0, 0, 0.0, width_inner, DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(angle_delta));
	immUnbindProgram();
}

static void dial_ghostarc_get_angles(
        struct Depsgraph *depsgraph,
        const wmManipulator *mpr,
        const wmEvent *event,
        const ARegion *ar, const View3D *v3d,
        float mat[4][4], const float co_outer[3],
        float *r_start, float *r_delta)
{
	DialInteraction *inter = mpr->interaction_data;
	const RegionView3D *rv3d = ar->regiondata;
	const float mval[2] = {event->x - ar->winrct.xmin, event->y - ar->winrct.ymin};

	/* we might need to invert the direction of the angles */
	float view_vec[3], axis_vec[3];
	ED_view3d_global_to_vector(rv3d, mpr->matrix_basis[3], view_vec);
	normalize_v3_v3(axis_vec, mpr->matrix_basis[2]);

	float proj_outer_rel[3];
	mul_v3_project_m4_v3(proj_outer_rel, mat, co_outer);
	sub_v3_v3(proj_outer_rel, mpr->matrix_basis[3]);

	float proj_mval_new_rel[3];
	float proj_mval_init_rel[3];
	float dial_plane[4];
	float ray_co[3], ray_no[3];
	float ray_lambda;

	plane_from_point_normal_v3(dial_plane, mpr->matrix_basis[3], axis_vec);

	if (!ED_view3d_win_to_ray(depsgraph, ar, v3d, inter->init_mval, ray_co, ray_no, false) ||
	    !isect_ray_plane_v3(ray_co, ray_no, dial_plane, &ray_lambda, false))
	{
		goto fail;
	}
	madd_v3_v3v3fl(proj_mval_init_rel, ray_co, ray_no, ray_lambda);
	sub_v3_v3(proj_mval_init_rel, mpr->matrix_basis[3]);

	if (!ED_view3d_win_to_ray(depsgraph, ar, v3d, mval, ray_co, ray_no, false) ||
	    !isect_ray_plane_v3(ray_co, ray_no, dial_plane, &ray_lambda, false))
	{
		goto fail;
	}
	madd_v3_v3v3fl(proj_mval_new_rel, ray_co, ray_no, ray_lambda);
	sub_v3_v3(proj_mval_new_rel, mpr->matrix_basis[3]);

	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");

	/* Start direction from mouse or set by user */
	const float *proj_init_rel =
	        (draw_options & ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_START_Y) ?
	        mpr->matrix_basis[1] : proj_mval_init_rel;

	/* return angles */
	const float start = angle_wrap_rad(angle_signed_on_axis_v3v3_v3(proj_outer_rel, proj_init_rel, axis_vec));
	const float delta = angle_wrap_rad(angle_signed_on_axis_v3v3_v3(proj_mval_init_rel, proj_mval_new_rel, axis_vec));

	/* Change of sign, we passed the 180 degree threshold. This means we need to add a turn
	 * to distinguish between transition from 0 to -1 and -PI to +PI, use comparison with PI/2.
	 * Logic taken from BLI_dial_angle */
	if ((delta * inter->last_angle < 0.0f) &&
	    (fabsf(inter->last_angle) > (float)M_PI_2))
	{
		if (inter->last_angle < 0.0f)
			inter->rotations--;
		else
			inter->rotations++;
	}
	inter->last_angle = delta;

	*r_start = start;
	*r_delta = fmod(delta + 2.0f * (float)M_PI * inter->rotations, 2 * (float)M_PI);
	return;

	/* If we can't project (unlikely). */
fail:
	*r_start = 0.0;
	*r_delta = 0.0;
}

static void dial_draw_intern(
        const bContext *C, wmManipulator *mpr,
        const bool select, const bool highlight, float clip_plane[4])
{
	float matrix_basis_adjust[4][4];
	float matrix_final[4][4];
	float color[4];

	BLI_assert(CTX_wm_area(C)->spacetype == SPACE_VIEW3D);

	manipulator_color_get(mpr, highlight, color);

	dial_calc_matrix(mpr, matrix_basis_adjust);

	WM_manipulator_calc_matrix_final_params(
	        mpr, &((struct WM_ManipulatorMatrixParams) {
	            .matrix_basis = (void *)matrix_basis_adjust,
	        }), matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	/* draw rotation indicator arc first */
	if ((mpr->flag & WM_MANIPULATOR_DRAW_VALUE) &&
	    (mpr->state & WM_MANIPULATOR_STATE_MODAL))
	{
		const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f}; /* coordinate at which the arc drawing will be started */

		DialInteraction *inter = mpr->interaction_data;

		/* XXX, View3D rotation manipulator doesn't call modal. */
		if (!WM_manipulator_target_property_is_valid_any(mpr)) {
			wmWindow *win = CTX_wm_window(C);
			manipulator_dial_modal((bContext *)C, mpr, win->eventstate, 0);
		}

		float angle_ofs = inter->output.angle_ofs;
		float angle_delta = inter->output.angle_delta;

		/* draw! */
		for (int i = 0; i < 2; i++) {
			dial_ghostarc_draw(mpr, angle_ofs, angle_delta, (const float [4]){0.8f, 0.8f, 0.8f, 0.4f});

			dial_ghostarc_draw_helpline(angle_ofs, co_outer, color); /* starting position */
			dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, color); /* starting position + current value */

			if (i == 0) {
				const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
				if ((draw_options & ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_MIRROR) == 0) {
					break;
				}
			}

			angle_ofs += (float)M_PI;
		}
	}

	/* draw actual dial manipulator */
	dial_geom_draw(mpr, color, select, matrix_basis_adjust, clip_plane);

	gpuPopMatrix();
}

static void manipulator_dial_draw_select(const bContext *C, wmManipulator *mpr, int select_id)
{
	float clip_plane_buf[4];
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
	float *clip_plane = (draw_options & ED_MANIPULATOR_DIAL_DRAW_FLAG_CLIP) ? clip_plane_buf : NULL;

	/* enable clipping if needed */
	if (clip_plane) {
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = ar->regiondata;

		copy_v3_v3(clip_plane, rv3d->viewinv[2]);
		clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], mpr->matrix_basis[3]);
		glEnable(GL_CLIP_DISTANCE0);
	}

	GPU_select_load_id(select_id);
	dial_draw_intern(C, mpr, true, false, clip_plane);

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

static void manipulator_dial_draw(const bContext *C, wmManipulator *mpr)
{
	const bool is_modal = mpr->state & WM_MANIPULATOR_STATE_MODAL;
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;
	float clip_plane_buf[4];
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
	float *clip_plane = (!is_modal && (draw_options & ED_MANIPULATOR_DIAL_DRAW_FLAG_CLIP)) ? clip_plane_buf : NULL;

	/* enable clipping if needed */
	if (clip_plane) {
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = ar->regiondata;

		copy_v3_v3(clip_plane, rv3d->viewinv[2]);
		clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], mpr->matrix_basis[3]);
		clip_plane[3] -= 0.02f * mpr->scale_final;

		glEnable(GL_CLIP_DISTANCE0);
	}

	glEnable(GL_BLEND);
	dial_draw_intern(C, mpr, false, is_highlight, clip_plane);
	glDisable(GL_BLEND);

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

static int manipulator_dial_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f}; /* coordinate at which the arc drawing will be started */
	float angle_ofs, angle_delta;

	float matrix[4][4];

	dial_calc_matrix(mpr, matrix);

	dial_ghostarc_get_angles(
	        CTX_data_depsgraph(C),
	        mpr, event, CTX_wm_region(C), CTX_wm_view3d(C), matrix, co_outer, &angle_ofs, &angle_delta);

	DialInteraction *inter = mpr->interaction_data;

	inter->output.angle_delta = angle_delta;
	inter->output.angle_ofs = angle_ofs;

	/* set the property for the operator and call its modal function */
	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		WM_manipulator_target_property_value_set(C, mpr, mpr_prop, inter->init_prop_angle + angle_delta);
	}
	return OPERATOR_RUNNING_MODAL;
}


static void manipulator_dial_setup(wmManipulator *mpr)
{
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	/* defaults */
	copy_v3_v3(mpr->matrix_basis[2], dir_default);
}

static int manipulator_dial_invoke(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	DialInteraction *inter = MEM_callocN(sizeof(DialInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	wmManipulatorProperty *mpr_prop = WM_manipulator_target_property_find(mpr, "offset");
	if (WM_manipulator_target_property_is_valid(mpr_prop)) {
		inter->init_prop_angle = WM_manipulator_target_property_value_get(mpr, mpr_prop);
	}

	mpr->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

/* -------------------------------------------------------------------- */
/** \name Dial Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_dial_3d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_dial_3d";

	/* api callbacks */
	wt->draw = manipulator_dial_draw;
	wt->draw_select = manipulator_dial_draw_select;
	wt->setup = manipulator_dial_setup;
	wt->invoke = manipulator_dial_invoke;
	wt->modal = manipulator_dial_modal;

	wt->struct_size = sizeof(wmManipulator);

	/* rna */
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_DIAL_DRAW_FLAG_CLIP, "CLIP", 0, "Clipped", ""},
		{ED_MANIPULATOR_DIAL_DRAW_FLAG_FILL, "FILL", 0, "Filled", ""},
		{ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_MIRROR, "ANGLE_MIRROR", 0, "Angle Mirror", ""},
		{ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_START_Y, "ANGLE_START_Y", 0, "Angle Start Y", ""},
		{0, NULL, 0, NULL, NULL}
	};
	RNA_def_enum_flag(wt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	WM_manipulatortype_target_property_def(wt, "offset", PROP_FLOAT, 1);
}

void ED_manipulatortypes_dial_3d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_dial_3d);
}

/** \} */
