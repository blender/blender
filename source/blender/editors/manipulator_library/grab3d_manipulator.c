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
 * 3D Manipulator
 *
 * \brief Simple manipulator to grab and translate.
 */

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

#include "GPU_select.h"

#include "GPU_matrix.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "manipulator_geometry.h"
#include "manipulator_library_intern.h"

static void manipulator_grab_modal(bContext *C, wmManipulator *mpr, const wmEvent *event, const int flag);

typedef struct GrabManipulator {
	wmManipulator manipulator;
	int style;
	float direction[3];
} GrabManipulator;

typedef struct GrabInteraction {
	float init_mval[2];

	/* only for when using properties */
	float init_prop_co[3];

	/* final output values, used for drawing */
	struct {
		float co_ofs[3];
		float co_final[3];
	} output;
} GrabInteraction;

#define DIAL_WIDTH       1.0f
#define DIAL_RESOLUTION 32


static void grab_calc_matrix(const GrabManipulator *grab, float mat[4][4])
{
	float rot[3][3];
	const float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, grab->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], grab->manipulator.matrix[3]);
	mul_mat3_m4_fl(mat, grab->manipulator.scale);
}

/* -------------------------------------------------------------------- */

static void grab_geom_draw(
        const GrabManipulator *grab3d, const float col[4], const bool select)
{
#ifdef USE_MANIPULATOR_CUSTOM_DIAL
	UNUSED_VARS(grab3d, col, axis_modal_mat);
	wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_grab3d, select);
#else
	const bool filled = (grab3d->style == ED_MANIPULATOR_DIAL_STYLE_RING_FILLED);

	glLineWidth(grab3d->manipulator.line_width);

	VertexFormat *format = immVertexFormat();
	uint pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(col);

	if (filled) {
		imm_draw_circle_fill(pos, 0, 0, 1.0, DIAL_RESOLUTION);
	}
	else {
		imm_draw_circle_wire(pos, 0, 0, 1.0, DIAL_RESOLUTION);
	}

	immUnbindProgram();

	UNUSED_VARS(select);
#endif
}

static void grab3d_get_translate(
        const GrabManipulator *grab, const wmEvent *event, const ARegion *ar,
        float co_delta[3])
{
	GrabInteraction *inter = grab->manipulator.interaction_data;
	const float mval_delta[2] = {
	    event->mval[0] - inter->init_mval[0],
	    event->mval[1] - inter->init_mval[1],
	};

	RegionView3D *rv3d = ar->regiondata;
	const float *co_ref = inter->init_prop_co;
	const float zfac = ED_view3d_calc_zfac(rv3d, co_ref, NULL);

	ED_view3d_win_to_delta(ar, mval_delta, co_delta, zfac);
}

static void grab3d_draw_intern(
        const bContext *C, GrabManipulator *grab,
        const bool select, const bool highlight)
{
	float mat[4][4];
	float col[4];

	BLI_assert(CTX_wm_area(C)->spacetype == SPACE_VIEW3D);

	manipulator_color_get(&grab->manipulator, highlight, col);

	grab_calc_matrix(grab, mat);

	gpuPushMatrix();
	if (grab->manipulator.interaction_data) {
		GrabInteraction *inter = grab->manipulator.interaction_data;
		gpuTranslate3fv(inter->output.co_ofs);
	}
	gpuMultMatrix(mat);
	gpuMultMatrix(grab->manipulator.matrix_offset);
	glEnable(GL_BLEND);
	grab_geom_draw(grab, col, select);
	glDisable(GL_BLEND);
	gpuPopMatrix();

	if (grab->manipulator.interaction_data) {
		gpuPushMatrix();
		gpuMultMatrix(mat);
		gpuMultMatrix(grab->manipulator.matrix_offset);
		glEnable(GL_BLEND);
		grab_geom_draw(grab, (const float [4]){0.5f, 0.5f, 0.5f, 0.5f}, select);
		glDisable(GL_BLEND);
		gpuPopMatrix();
	}
}

static void manipulator_grab_draw_select(const bContext *C, wmManipulator *mpr, int selectionbase)
{
	GrabManipulator *grab = (GrabManipulator *)mpr;

	GPU_select_load_id(selectionbase);
	grab3d_draw_intern(C, grab, true, false);
}

static void manipulator_grab_draw(const bContext *C, wmManipulator *mpr)
{
	GrabManipulator *grab = (GrabManipulator *)mpr;
	const bool active = mpr->state & WM_MANIPULATOR_STATE_ACTIVE;
	const bool highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	BLI_assert(grab->style != -1);

	(void)active;

	glEnable(GL_BLEND);
	grab3d_draw_intern(C, grab, false, highlight);
	glDisable(GL_BLEND);
}

static void manipulator_grab_modal(bContext *C, wmManipulator *mpr, const wmEvent *event, const int UNUSED(flag))
{
	GrabManipulator *grab = (GrabManipulator *)mpr;

	GrabInteraction *inter = grab->manipulator.interaction_data;

	grab3d_get_translate(grab, event, CTX_wm_region(C), inter->output.co_ofs);

	add_v3_v3v3(inter->output.co_final, inter->init_prop_co, inter->output.co_ofs);

	/* set the property for the operator and call its modal function */
	wmManipulatorProperty *mpr_prop = WM_manipulator_property_find(mpr, "offset");
	if (mpr_prop && WM_manipulator_property_is_valid(mpr_prop)) {
		WM_manipulator_property_value_set_array(C, mpr, mpr_prop, inter->output.co_final, 3);
	}
}

static void manipulator_grab_setup(wmManipulator *mpr)
{
	GrabManipulator *grab = (GrabManipulator *)mpr;

	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	grab->style = -1;

	/* defaults */
	copy_v3_v3(grab->direction, dir_default);
}

static void manipulator_grab_invoke(
        bContext *UNUSED(C), wmManipulator *mpr, const wmEvent *event)
{
	GrabInteraction *inter = MEM_callocN(sizeof(GrabInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	wmManipulatorProperty *mpr_prop = WM_manipulator_property_find(mpr, "offset");
	if (mpr_prop && WM_manipulator_property_is_valid(mpr_prop)) {
		WM_manipulator_property_value_get_array(mpr, mpr_prop, inter->init_prop_co, 3);
	}

	mpr->interaction_data = inter;
}

/* -------------------------------------------------------------------- */
/** \name Grab Manipulator API
 *
 * \{ */

#define ASSERT_TYPE_CHECK(mpr) BLI_assert(mpr->type->draw == manipulator_grab_draw)

void ED_manipulator_grab3d_set_style(wmManipulator *mpr, int style)
{
	ASSERT_TYPE_CHECK(mpr);
	GrabManipulator *grab = (GrabManipulator *)mpr;
	grab->style = style;
}

/**
 * Define up-direction of the grab3d manipulator
 */
void ED_manipulator_grab3d_set_up_vector(wmManipulator *mpr, const float direction[3])
{
	ASSERT_TYPE_CHECK(mpr);
	GrabManipulator *grab = (GrabManipulator *)mpr;

	copy_v3_v3(grab->direction, direction);
	normalize_v3(grab->direction);
}

static void MANIPULATOR_WT_grab_3d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_grab_3d";

	/* api callbacks */
	wt->draw = manipulator_grab_draw;
	wt->draw_select = manipulator_grab_draw_select;
	wt->setup = manipulator_grab_setup;
	wt->invoke = manipulator_grab_invoke;
	wt->modal = manipulator_grab_modal;

	wt->struct_size = sizeof(GrabManipulator);
}

void ED_manipulatortypes_grab_3d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_grab_3d);
}

/** \} */ // Grab Manipulator API
