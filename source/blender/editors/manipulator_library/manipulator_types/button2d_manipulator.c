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
 * 2D Manipulator, also works in 3D views.
 *
 * \brief Single click button action for use in manipulator groups.
 *
 * \note Currently only basic icon & vector-shape buttons are supported.
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
#include "GPU_batch.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

/* own includes */
#include "../manipulator_geometry.h"
#include "../manipulator_library_intern.h"

typedef struct ButtonManipulator2D {
	wmManipulator manipulator;
	bool is_init;
	/* Use an icon or shape */
	int icon;
	Gwn_Batch *shape_batch[2];
} ButtonManipulator2D;

#define CIRCLE_RESOLUTION 32

/* -------------------------------------------------------------------- */

static void button2d_geom_draw_backdrop(
        const wmManipulator *mpr, const float color[4], const bool select)
{
	glLineWidth(mpr->line_width);

	Gwn_VertFormat *format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	/* TODO, other draw styles */
	imm_draw_circle_fill_2d(pos, 0, 0, 1.0f, CIRCLE_RESOLUTION);

	immUnbindProgram();

	UNUSED_VARS(select);
}

static void button2d_draw_intern(
        const bContext *C, wmManipulator *mpr,
        const bool select, const bool highlight)
{
	ButtonManipulator2D *button = (ButtonManipulator2D *)mpr;

	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");
	if (button->is_init == false) {
		button->is_init = true;
		PropertyRNA *prop = RNA_struct_find_property(mpr->ptr, "icon");
		if (RNA_property_is_set(mpr->ptr, prop)) {
			button->icon = RNA_property_enum_get(mpr->ptr, prop);
		}
		else {
			prop = RNA_struct_find_property(mpr->ptr, "shape");
			const uint polys_len = RNA_property_string_length(mpr->ptr, prop);
			/* We shouldn't need the +1, but a NULL char is set. */
			char *polys = MEM_mallocN(polys_len + 1, __func__);
			RNA_property_string_get(mpr->ptr, prop, polys);
			button->shape_batch[0] = GPU_batch_tris_from_poly_2d_encoded((uchar *)polys, polys_len, NULL);
			button->shape_batch[1] = GPU_batch_wire_from_poly_2d_encoded((uchar *)polys, polys_len, NULL);
			MEM_freeN(polys);
		}
	}

	float color[4];
	float matrix_final[4][4];

	manipulator_color_get(mpr, highlight, color);
	WM_manipulator_calc_matrix_final(mpr, matrix_final);


	bool is_3d = (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) != 0;


	if (draw_options & ED_MANIPULATOR_BUTTON_SHOW_HELPLINE) {
		float matrix_final_no_offset[4][4];
		WM_manipulator_calc_matrix_final_no_offset(mpr, matrix_final_no_offset);
		uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv(color);
		glLineWidth(mpr->line_width);
		immUniformColor4fv(color);
		immBegin(GWN_PRIM_LINE_STRIP, 2);
		immVertex3fv(pos, matrix_final[3]);
		immVertex3fv(pos, matrix_final_no_offset[3]);
		immEnd();
		immUnbindProgram();
	}

	bool need_to_pop = true;
	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	if (is_3d) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		float matrix_align[4][4];
		float matrix_final_unit[4][4];
		normalize_m4_m4(matrix_final_unit, matrix_final);
		mul_m4_m4m4(matrix_align, rv3d->viewmat, matrix_final_unit);
		zero_v3(matrix_align[3]);
		transpose_m4(matrix_align);
		gpuMultMatrix(matrix_align);
	}

	if (select) {
		BLI_assert(is_3d);
		button2d_geom_draw_backdrop(mpr, color, select);
	}
	else {

		glEnable(GL_BLEND);
		if (button->shape_batch[0] != NULL) {
			glEnable(GL_LINE_SMOOTH);
			glDisable(GL_POLYGON_SMOOTH);
			glLineWidth(1.0f);
			for (uint i = 0; i < ARRAY_SIZE(button->shape_batch) && button->shape_batch[i]; i++) {
				/* Invert line color for wire. */
				GWN_batch_program_set_builtin(button->shape_batch[i], GPU_SHADER_2D_UNIFORM_COLOR);
				GWN_batch_uniform_4f(button->shape_batch[i], "color", UNPACK4(color));
				GWN_batch_draw(button->shape_batch[i]);

				if (draw_options & ED_MANIPULATOR_BUTTON_SHOW_OUTLINE) {
					color[0] = 1.0f - color[0];
					color[1] = 1.0f - color[1];
					color[2] = 1.0f - color[2];
				}
			}
			glDisable(GL_LINE_SMOOTH);
			glEnable(GL_POLYGON_SMOOTH);
		}
		else if (button->icon != ICON_NONE) {
			button2d_geom_draw_backdrop(mpr, color, select);
			float size[2];
			if (is_3d) {
				const float fac = 2.0f;
				gpuTranslate2f(-(fac / 2), -(fac / 2));
				gpuScale2f(fac / (ICON_DEFAULT_WIDTH *  UI_DPI_FAC), fac / (ICON_DEFAULT_HEIGHT * UI_DPI_FAC));
				size[0] = 1.0f;
				size[1] = 1.0f;
			}
			else {
				size[0] = mpr->matrix_basis[3][0] - (ICON_DEFAULT_WIDTH / 2.0) * UI_DPI_FAC;
				size[1] = mpr->matrix_basis[3][1] - (ICON_DEFAULT_HEIGHT / 2.0) * UI_DPI_FAC;
				gpuPopMatrix();
				need_to_pop = false;
			}
			UI_icon_draw(size[0], size[1], button->icon);
		}
		glDisable(GL_BLEND);
	}

	if (need_to_pop) {
		gpuPopMatrix();
	}
}

static void manipulator_button2d_draw_select(const bContext *C, wmManipulator *mpr, int select_id)
{
	GPU_select_load_id(select_id);
	button2d_draw_intern(C, mpr, true, false);
}

static void manipulator_button2d_draw(const bContext *C, wmManipulator *mpr)
{
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	glEnable(GL_BLEND);
	button2d_draw_intern(C, mpr, false, is_highlight);
	glDisable(GL_BLEND);
}

static int manipulator_button2d_test_select(
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
		copy_v2_v2(point_local, (float[2]){UNPACK2(event->mval)});
		sub_v2_v2(point_local, mpr->matrix_basis[3]);
		mul_v2_fl(point_local, 1.0f / (mpr->scale_basis * UI_DPI_FAC));
	}
	/* The 'mpr->scale_final' is already applied when projecting. */
	if (len_squared_v2(point_local) < 1.0f) {
		return 0;
	}

	return -1;
}

static int manipulator_button2d_cursor_get(wmManipulator *mpr)
{
	if (RNA_boolean_get(mpr->ptr, "show_drag")) {
		return BC_NSEW_SCROLLCURSOR;
	}
	return CURSOR_STD;
}

static void manipulator_button2d_free(wmManipulator *mpr)
{
	ButtonManipulator2D *shape = (ButtonManipulator2D *)mpr;

	for (uint i = 0; i < ARRAY_SIZE(shape->shape_batch); i++) {
		GWN_BATCH_DISCARD_SAFE(shape->shape_batch[i]);
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_button_2d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_button_2d";

	/* api callbacks */
	wt->draw = manipulator_button2d_draw;
	wt->draw_select = manipulator_button2d_draw_select;
	wt->test_select = manipulator_button2d_test_select;
	wt->cursor_get = manipulator_button2d_cursor_get;
	wt->free = manipulator_button2d_free;

	wt->struct_size = sizeof(ButtonManipulator2D);

	/* rna */
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_BUTTON_SHOW_OUTLINE, "OUTLINE", 0, "Outline", ""},
		{ED_MANIPULATOR_BUTTON_SHOW_HELPLINE, "HELPLINE", 0, "Help Line", ""},
		{0, NULL, 0, NULL, NULL}
	};
	PropertyRNA *prop;

	RNA_def_enum_flag(wt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	prop = RNA_def_property(wt->srna, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_icon_items);

	/* Passed to 'GPU_batch_tris_from_poly_2d_encoded' */
	RNA_def_property(wt->srna, "shape", PROP_STRING, PROP_BYTESTRING);

	/* Currently only used for cursor display. */
	RNA_def_boolean(wt->srna, "show_drag", true, "Show Drag", "");
}

void ED_manipulatortypes_button_2d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_button_2d);
}

/** \} */ // Button Manipulator API
