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

/** \file dial3d_gizmo.c
 *  \ingroup wm
 *
 * \name Dial Gizmo
 *
 * 3D Gizmo
 *
 * \brief Circle shaped gizmo for circular interaction.
 * Currently no own handling, use with operator only.
 *
 * - `matrix[0]` is derived from Y and Z.
 * - `matrix[1]` is 'up' when DialGizmo.use_start_y_axis is set.
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
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

/* to use custom dials exported to geom_dial_gizmo.c */
// #define USE_GIZMO_CUSTOM_DIAL

static int gizmo_dial_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak tweak_flag);

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
#define DIAL_RESOLUTION 48

/* Could make option, negative to clip more (don't show when view aligned). */
#define DIAL_CLIP_BIAS 0.02

/**
 * We can't use this for the #wmGizmoType.matrix_basis_get callback, it conflicts with depth picking.
 */
static void dial_calc_matrix(const wmGizmo *gz, float mat[4][4])
{
	float rot[3][3];
	const float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, gz->matrix_basis[2]);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], gz->matrix_basis[3]);
}

/* -------------------------------------------------------------------- */

static void dial_geom_draw(
        const wmGizmo *gz, const float color[4], const bool select,
        float axis_modal_mat[4][4], float clip_plane[4])
{
#ifdef USE_GIZMO_CUSTOM_DIAL
	UNUSED_VARS(dial, col, axis_modal_mat, clip_plane);
	wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_dial, select);
#else
	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
	const bool filled = (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_FILL) != 0;

	GPU_line_width(gz->line_width);

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

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
	GPU_line_width(1.0f);

	GPU_matrix_push();
	GPU_matrix_rotate_3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GPU_PRIM_LINE_STRIP, 2);
	immVertex3f(pos, 0.0f, 0.0f, 0.0f);
	immVertex3fv(pos, co_outer);
	immEnd();

	immUnbindProgram();

	GPU_matrix_pop();
}

static void dial_ghostarc_draw(
        const wmGizmo *gz, const float angle_ofs, const float angle_delta, const float color[4])
{
	const float width_inner = DIAL_WIDTH - gz->line_width * 0.5f / U.gizmo_size;

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	imm_draw_disk_partial_fill_2d(
	        pos, 0, 0, 0.0, width_inner, DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(angle_delta));
	immUnbindProgram();
}

static void dial_ghostarc_get_angles(
        struct Depsgraph *depsgraph,
        const wmGizmo *gz,
        const wmEvent *event,
        const ARegion *ar, const View3D *v3d,
        float mat[4][4], const float co_outer[3],
        float *r_start, float *r_delta)
{
	DialInteraction *inter = gz->interaction_data;
	const RegionView3D *rv3d = ar->regiondata;
	const float mval[2] = {event->x - ar->winrct.xmin, event->y - ar->winrct.ymin};

	/* we might need to invert the direction of the angles */
	float view_vec[3], axis_vec[3];
	ED_view3d_global_to_vector(rv3d, gz->matrix_basis[3], view_vec);
	normalize_v3_v3(axis_vec, gz->matrix_basis[2]);

	float proj_outer_rel[3];
	mul_v3_project_m4_v3(proj_outer_rel, mat, co_outer);
	sub_v3_v3(proj_outer_rel, gz->matrix_basis[3]);

	float proj_mval_new_rel[3];
	float proj_mval_init_rel[3];
	float dial_plane[4];
	float ray_co[3], ray_no[3];
	float ray_lambda;

	plane_from_point_normal_v3(dial_plane, gz->matrix_basis[3], axis_vec);

	if (!ED_view3d_win_to_ray(depsgraph, ar, v3d, inter->init_mval, ray_co, ray_no, false) ||
	    !isect_ray_plane_v3(ray_co, ray_no, dial_plane, &ray_lambda, false))
	{
		goto fail;
	}
	madd_v3_v3v3fl(proj_mval_init_rel, ray_co, ray_no, ray_lambda);
	sub_v3_v3(proj_mval_init_rel, gz->matrix_basis[3]);

	if (!ED_view3d_win_to_ray(depsgraph, ar, v3d, mval, ray_co, ray_no, false) ||
	    !isect_ray_plane_v3(ray_co, ray_no, dial_plane, &ray_lambda, false))
	{
		goto fail;
	}
	madd_v3_v3v3fl(proj_mval_new_rel, ray_co, ray_no, ray_lambda);
	sub_v3_v3(proj_mval_new_rel, gz->matrix_basis[3]);

	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");

	/* Start direction from mouse or set by user */
	const float *proj_init_rel =
	        (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y) ?
	        gz->matrix_basis[1] : proj_mval_init_rel;

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
        const bContext *C, wmGizmo *gz,
        const bool select, const bool highlight, float clip_plane[4])
{
	float matrix_basis_adjust[4][4];
	float matrix_final[4][4];
	float color[4];

	BLI_assert(CTX_wm_area(C)->spacetype == SPACE_VIEW3D);

	gizmo_color_get(gz, highlight, color);

	dial_calc_matrix(gz, matrix_basis_adjust);

	WM_gizmo_calc_matrix_final_params(
	        gz, &((struct WM_GizmoMatrixParams) {
	            .matrix_basis = (void *)matrix_basis_adjust,
	        }), matrix_final);

	GPU_matrix_push();
	GPU_matrix_mul(matrix_final);

	/* draw rotation indicator arc first */
	if ((gz->flag & WM_GIZMO_DRAW_VALUE) &&
	    (gz->state & WM_GIZMO_STATE_MODAL))
	{
		const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f}; /* coordinate at which the arc drawing will be started */

		DialInteraction *inter = gz->interaction_data;

		/* XXX, View3D rotation gizmo doesn't call modal. */
		if (!WM_gizmo_target_property_is_valid_any(gz)) {
			wmWindow *win = CTX_wm_window(C);
			gizmo_dial_modal((bContext *)C, gz, win->eventstate, 0);
		}

		float angle_ofs = inter->output.angle_ofs;
		float angle_delta = inter->output.angle_delta;

		/* draw! */
		for (int i = 0; i < 2; i++) {
			GPU_polygon_smooth(false);
			dial_ghostarc_draw(gz, angle_ofs, angle_delta, (const float[4]){0.8f, 0.8f, 0.8f, 0.4f});
			GPU_polygon_smooth(true);

			dial_ghostarc_draw_helpline(angle_ofs, co_outer, color); /* starting position */
			dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, color); /* starting position + current value */

			if (i == 0) {
				const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
				if ((draw_options & ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR) == 0) {
					break;
				}
			}

			angle_ofs += (float)M_PI;
		}
	}

	/* draw actual dial gizmo */
	dial_geom_draw(gz, color, select, matrix_basis_adjust, clip_plane);

	GPU_matrix_pop();
}

static void gizmo_dial_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
	float clip_plane_buf[4];
	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
	float *clip_plane = (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_CLIP) ? clip_plane_buf : NULL;

	/* enable clipping if needed */
	if (clip_plane) {
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = ar->regiondata;

		copy_v3_v3(clip_plane, rv3d->viewinv[2]);
		clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], gz->matrix_basis[3]);
		clip_plane[3] += DIAL_CLIP_BIAS * gz->scale_final;
		glEnable(GL_CLIP_DISTANCE0);
	}

	GPU_select_load_id(select_id);
	dial_draw_intern(C, gz, true, false, clip_plane);

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

static void gizmo_dial_draw(const bContext *C, wmGizmo *gz)
{
	const bool is_modal = gz->state & WM_GIZMO_STATE_MODAL;
	const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;
	float clip_plane_buf[4];
	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
	float *clip_plane = (!is_modal && (draw_options & ED_GIZMO_DIAL_DRAW_FLAG_CLIP)) ? clip_plane_buf : NULL;

	/* enable clipping if needed */
	if (clip_plane) {
		ARegion *ar = CTX_wm_region(C);
		RegionView3D *rv3d = ar->regiondata;

		copy_v3_v3(clip_plane, rv3d->viewinv[2]);
		clip_plane[3] = -dot_v3v3(rv3d->viewinv[2], gz->matrix_basis[3]);
		clip_plane[3] += DIAL_CLIP_BIAS * gz->scale_final;

		glEnable(GL_CLIP_DISTANCE0);
	}

	GPU_blend(true);
	dial_draw_intern(C, gz, false, is_highlight, clip_plane);
	GPU_blend(false);

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

static int gizmo_dial_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
	const float co_outer[4] = {0.0f, DIAL_WIDTH, 0.0f}; /* coordinate at which the arc drawing will be started */
	float angle_ofs, angle_delta;

	float matrix[4][4];

	dial_calc_matrix(gz, matrix);

	dial_ghostarc_get_angles(
	        CTX_data_depsgraph(C),
	        gz, event, CTX_wm_region(C), CTX_wm_view3d(C), matrix, co_outer, &angle_ofs, &angle_delta);

	DialInteraction *inter = gz->interaction_data;

	inter->output.angle_delta = angle_delta;
	inter->output.angle_ofs = angle_ofs;

	/* set the property for the operator and call its modal function */
	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		WM_gizmo_target_property_value_set(C, gz, gz_prop, inter->init_prop_angle + angle_delta);
	}
	return OPERATOR_RUNNING_MODAL;
}


static void gizmo_dial_setup(wmGizmo *gz)
{
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	/* defaults */
	copy_v3_v3(gz->matrix_basis[2], dir_default);
}

static int gizmo_dial_invoke(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
	DialInteraction *inter = MEM_callocN(sizeof(DialInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		inter->init_prop_angle = WM_gizmo_target_property_value_get(gz, gz_prop);
	}

	gz->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

/* -------------------------------------------------------------------- */
/** \name Dial Gizmo API
 *
 * \{ */

static void GIZMO_GT_dial_3d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_dial_3d";

	/* api callbacks */
	gzt->draw = gizmo_dial_draw;
	gzt->draw_select = gizmo_dial_draw_select;
	gzt->setup = gizmo_dial_setup;
	gzt->invoke = gizmo_dial_invoke;
	gzt->modal = gizmo_dial_modal;

	gzt->struct_size = sizeof(wmGizmo);

	/* rna */
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_GIZMO_DIAL_DRAW_FLAG_CLIP, "CLIP", 0, "Clipped", ""},
		{ED_GIZMO_DIAL_DRAW_FLAG_FILL, "FILL", 0, "Filled", ""},
		{ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR, "ANGLE_MIRROR", 0, "Angle Mirror", ""},
		{ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y, "ANGLE_START_Y", 0, "Angle Start Y", ""},
		{0, NULL, 0, NULL, NULL}
	};
	RNA_def_enum_flag(gzt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	WM_gizmotype_target_property_def(gzt, "offset", PROP_FLOAT, 1);
}

void ED_gizmotypes_dial_3d(void)
{
	WM_gizmotype_append(GIZMO_GT_dial_3d);
}

/** \} */
