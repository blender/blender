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

/** \file button2d_manipulator.c
 *  \ingroup wm
 *
 * \name Button Manipulator
 *
 * 3D Manipulator, also works in 2D views.
 *
 * \brief Single click button action for use in manipulator groups.
 *
 * Note: currently only very basic icon buttons supported.
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
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"

/* own includes */
#include "../manipulator_geometry.h"
#include "../manipulator_library_intern.h"

typedef struct ButtonManipulator3D {
	wmManipulator manipulator;
	/* Added to 'matrix_basis' when calculating the matrix. */
	float prop_co[3];
} ButtonManipulator3D;

#define DIAL_RESOLUTION 32

/* -------------------------------------------------------------------- */

static void button_geom_draw(
        const wmManipulator *mpr, const float color[4], const bool select)
{
	glLineWidth(mpr->line_width);

	Gwn_VertFormat *format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	/* TODO, other draw styles */
	imm_draw_circle_fill_2d(pos, 0, 0, 1.0f, DIAL_RESOLUTION);

	immUnbindProgram();

	UNUSED_VARS(select);
}

static void button3d_draw_intern(
        const bContext *UNUSED(C), wmManipulator *mpr,
        const bool select, const bool highlight)
{
	float color[4];
	float matrix_final[4][4];

	manipulator_color_get(mpr, highlight, color);
	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	glEnable(GL_BLEND);
	button_geom_draw(mpr, color, select);
	gpuPopMatrix();


	if (select == false) {
		int icon = RNA_enum_get(mpr->ptr, "icon");
		if (icon != ICON_NONE) {
			UI_icon_draw(
			        mpr->matrix_basis[3][0] - (ICON_DEFAULT_WIDTH / 2.0) * U.ui_scale,
			        mpr->matrix_basis[3][1] - (ICON_DEFAULT_HEIGHT / 2.0) * U.ui_scale,
			        icon);
		}
	}
	glDisable(GL_BLEND);
}

static void manipulator_button_draw_select(const bContext *C, wmManipulator *mpr, int select_id)
{
	GPU_select_load_id(select_id);
	button3d_draw_intern(C, mpr, true, false);
}

static void manipulator_button_draw(const bContext *C, wmManipulator *mpr)
{
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	glEnable(GL_BLEND);
	button3d_draw_intern(C, mpr, false, is_highlight);
	glDisable(GL_BLEND);
}

static int manipulator_button_test_select(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	float point_local[2];

	if (0) {
		/* correct, but unnecessarily slow. */
		if (manipulator_window_project_2d(
		        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, true, point_local) == false)
		{
			return -1;
		}
	}
	else {
		copy_v2_v2(point_local, (float [2]){UNPACK2(event->mval)});
		sub_v2_v2(point_local, mpr->matrix_basis[3]);
		mul_v2_fl(point_local, 1.0f / (mpr->scale_basis * U.ui_scale));
	}
	/* The 'mpr->scale_final' is already applied when projecting. */
	if (len_squared_v2(point_local) < 1.0f) {
		return 0;
	}

	return -1;
}

static int manipulator_button_cursor_get(wmManipulator *UNUSED(mpr))
{
	return BC_HANDCURSOR;
}

/* -------------------------------------------------------------------- */
/** \name Button Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_button_2d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_button_2d";

	/* api callbacks */
	wt->draw = manipulator_button_draw;
	wt->draw_select = manipulator_button_draw_select;
	wt->test_select = manipulator_button_test_select;
	wt->cursor_get = manipulator_button_cursor_get;

	wt->struct_size = sizeof(ButtonManipulator3D);

	/* rna */
	PropertyRNA *prop;
	prop = RNA_def_property(wt->srna, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_icon_items);
	RNA_def_property_ui_text(prop, "Icon", "Override automatic icon of the item");
}

void ED_manipulatortypes_button_2d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_button_2d);
}

/** \} */ // Button Manipulator API
