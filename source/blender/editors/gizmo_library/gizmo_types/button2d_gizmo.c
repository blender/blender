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

/** \file button2d_gizmo.c
 *  \ingroup wm
 *
 * \name Button Gizmo
 *
 * 2D Gizmo, also works in 3D views.
 *
 * \brief Single click button action for use in gizmo groups.
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
#include "GPU_batch_utils.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

typedef struct ButtonGizmo2D {
	wmGizmo gizmo;
	bool is_init;
	/* Use an icon or shape */
	int icon;
	GPUBatch *shape_batch[2];
} ButtonGizmo2D;

#define CIRCLE_RESOLUTION 32

/* -------------------------------------------------------------------- */

static void button2d_geom_draw_backdrop(
        const wmGizmo *gz, const float color[4], const bool select)
{
	GPU_line_width(gz->line_width);

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	/* TODO, other draw styles */
	imm_draw_circle_fill_2d(pos, 0, 0, 1.0f, CIRCLE_RESOLUTION);

	immUnbindProgram();

	UNUSED_VARS(select);
}

static void button2d_draw_intern(
        const bContext *C, wmGizmo *gz,
        const bool select, const bool highlight)
{
	ButtonGizmo2D *button = (ButtonGizmo2D *)gz;

	const int draw_options = RNA_enum_get(gz->ptr, "draw_options");
	if (button->is_init == false) {
		button->is_init = true;
		PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
		if (RNA_property_is_set(gz->ptr, prop)) {
			button->icon = RNA_property_enum_get(gz->ptr, prop);
		}
		else {
			prop = RNA_struct_find_property(gz->ptr, "shape");
			const uint polys_len = RNA_property_string_length(gz->ptr, prop);
			/* We shouldn't need the +1, but a NULL char is set. */
			char *polys = MEM_mallocN(polys_len + 1, __func__);
			RNA_property_string_get(gz->ptr, prop, polys);
			button->shape_batch[0] = GPU_batch_tris_from_poly_2d_encoded((uchar *)polys, polys_len, NULL);
			button->shape_batch[1] = GPU_batch_wire_from_poly_2d_encoded((uchar *)polys, polys_len, NULL);
			MEM_freeN(polys);
		}
	}

	float color[4];
	float matrix_final[4][4];

	gizmo_color_get(gz, highlight, color);
	WM_gizmo_calc_matrix_final(gz, matrix_final);


	bool is_3d = (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) != 0;


	if (draw_options & ED_GIZMO_BUTTON_SHOW_HELPLINE) {
		float matrix_final_no_offset[4][4];
		WM_gizmo_calc_matrix_final_no_offset(gz, matrix_final_no_offset);
		uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		immUniformColor4fv(color);
		GPU_line_width(gz->line_width);
		immUniformColor4fv(color);
		immBegin(GPU_PRIM_LINE_STRIP, 2);
		immVertex3fv(pos, matrix_final[3]);
		immVertex3fv(pos, matrix_final_no_offset[3]);
		immEnd();
		immUnbindProgram();
	}

	bool need_to_pop = true;
	GPU_matrix_push();
	GPU_matrix_mul(matrix_final);

	if (is_3d) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		float matrix_align[4][4];
		float matrix_final_unit[4][4];
		normalize_m4_m4(matrix_final_unit, matrix_final);
		mul_m4_m4m4(matrix_align, rv3d->viewmat, matrix_final_unit);
		zero_v3(matrix_align[3]);
		transpose_m4(matrix_align);
		GPU_matrix_mul(matrix_align);
	}

	if (select) {
		BLI_assert(is_3d);
		button2d_geom_draw_backdrop(gz, color, select);
	}
	else {

		GPU_blend(true);
		if (button->shape_batch[0] != NULL) {
			GPU_line_smooth(true);
			GPU_polygon_smooth(false);
			GPU_line_width(1.0f);
			for (uint i = 0; i < ARRAY_SIZE(button->shape_batch) && button->shape_batch[i]; i++) {
				/* Invert line color for wire. */
				GPU_batch_program_set_builtin(button->shape_batch[i], GPU_SHADER_2D_UNIFORM_COLOR);
				GPU_batch_uniform_4f(button->shape_batch[i], "color", UNPACK4(color));
				GPU_batch_draw(button->shape_batch[i]);

				if (draw_options & ED_GIZMO_BUTTON_SHOW_OUTLINE) {
					color[0] = 1.0f - color[0];
					color[1] = 1.0f - color[1];
					color[2] = 1.0f - color[2];
				}
			}
			GPU_line_smooth(false);
			GPU_polygon_smooth(true);
		}
		else if (button->icon != ICON_NONE) {
			button2d_geom_draw_backdrop(gz, color, select);
			float size[2];
			if (is_3d) {
				const float fac = 2.0f;
				GPU_matrix_translate_2f(-(fac / 2), -(fac / 2));
				GPU_matrix_scale_2f(fac / (ICON_DEFAULT_WIDTH *  UI_DPI_FAC), fac / (ICON_DEFAULT_HEIGHT * UI_DPI_FAC));
				size[0] = 1.0f;
				size[1] = 1.0f;
			}
			else {
				size[0] = gz->matrix_basis[3][0] - (ICON_DEFAULT_WIDTH / 2.0) * UI_DPI_FAC;
				size[1] = gz->matrix_basis[3][1] - (ICON_DEFAULT_HEIGHT / 2.0) * UI_DPI_FAC;
				GPU_matrix_pop();
				need_to_pop = false;
			}
			UI_icon_draw(size[0], size[1], button->icon);
		}
		GPU_blend(false);
	}

	if (need_to_pop) {
		GPU_matrix_pop();
	}
}

static void gizmo_button2d_draw_select(const bContext *C, wmGizmo *gz, int select_id)
{
	GPU_select_load_id(select_id);
	button2d_draw_intern(C, gz, true, false);
}

static void gizmo_button2d_draw(const bContext *C, wmGizmo *gz)
{
	const bool is_highlight = (gz->state & WM_GIZMO_STATE_HIGHLIGHT) != 0;

	GPU_blend(true);
	button2d_draw_intern(C, gz, false, is_highlight);
	GPU_blend(false);
}

static int gizmo_button2d_test_select(
        bContext *C, wmGizmo *gz, const int mval[2])
{
	float point_local[2];

	if (0) {
		/* correct, but unnecessarily slow. */
		if (gizmo_window_project_2d(
		        C, gz, (const float[2]){UNPACK2(mval)}, 2, true, point_local) == false)
		{
			return -1;
		}
	}
	else {
		copy_v2_v2(point_local, (float[2]){UNPACK2(mval)});
		sub_v2_v2(point_local, gz->matrix_basis[3]);
		mul_v2_fl(point_local, 1.0f / (gz->scale_basis * UI_DPI_FAC));
	}
	/* The 'gz->scale_final' is already applied when projecting. */
	if (len_squared_v2(point_local) < 1.0f) {
		return 0;
	}

	return -1;
}

static int gizmo_button2d_cursor_get(wmGizmo *gz)
{
	if (RNA_boolean_get(gz->ptr, "show_drag")) {
		return BC_NSEW_SCROLLCURSOR;
	}
	return CURSOR_STD;
}

static void gizmo_button2d_free(wmGizmo *gz)
{
	ButtonGizmo2D *shape = (ButtonGizmo2D *)gz;

	for (uint i = 0; i < ARRAY_SIZE(shape->shape_batch); i++) {
		GPU_BATCH_DISCARD_SAFE(shape->shape_batch[i]);
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Gizmo API
 *
 * \{ */

static void GIZMO_GT_button_2d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_button_2d";

	/* api callbacks */
	gzt->draw = gizmo_button2d_draw;
	gzt->draw_select = gizmo_button2d_draw_select;
	gzt->test_select = gizmo_button2d_test_select;
	gzt->cursor_get = gizmo_button2d_cursor_get;
	gzt->free = gizmo_button2d_free;

	gzt->struct_size = sizeof(ButtonGizmo2D);

	/* rna */
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_GIZMO_BUTTON_SHOW_OUTLINE, "OUTLINE", 0, "Outline", ""},
		{ED_GIZMO_BUTTON_SHOW_HELPLINE, "HELPLINE", 0, "Help Line", ""},
		{0, NULL, 0, NULL, NULL}
	};
	PropertyRNA *prop;

	RNA_def_enum_flag(gzt->srna, "draw_options", rna_enum_draw_options, 0, "Draw Options", "");

	prop = RNA_def_property(gzt->srna, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rna_enum_icon_items);

	/* Passed to 'GPU_batch_tris_from_poly_2d_encoded' */
	RNA_def_property(gzt->srna, "shape", PROP_STRING, PROP_BYTESTRING);

	/* Currently only used for cursor display. */
	RNA_def_boolean(gzt->srna, "show_drag", true, "Show Drag", "");
}

void ED_gizmotypes_button_2d(void)
{
	WM_gizmotype_append(GIZMO_GT_button_2d);
}

/** \} */ // Button Gizmo API
