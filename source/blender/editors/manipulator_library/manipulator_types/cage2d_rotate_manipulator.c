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

/** \file cage2d_manipulator.c
 *  \ingroup wm
 *
 * \name Cage Manipulator
 *
 * 2D Manipulator
 *
 * \brief Rectangular manipulator acting as a 'cage' around its content.
 * Interacting scales or translates the manipulator.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "../manipulator_library_intern.h"

#define MANIPULATOR_RESIZER_WIDTH  20.0f

/* -------------------------------------------------------------------- */

static void rect_transform_draw_corners(
        const rctf *r, const float offsetx, const float offsety, const float color[3])
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	immBegin(GWN_PRIM_LINES, 16);

	immVertex2f(pos, r->xmin, r->ymin + offsety);
	immVertex2f(pos, r->xmin, r->ymin);
	immVertex2f(pos, r->xmin, r->ymin);
	immVertex2f(pos, r->xmin + offsetx, r->ymin);

	immVertex2f(pos, r->xmax, r->ymin + offsety);
	immVertex2f(pos, r->xmax, r->ymin);
	immVertex2f(pos, r->xmax, r->ymin);
	immVertex2f(pos, r->xmax - offsetx, r->ymin);

	immVertex2f(pos, r->xmax, r->ymax - offsety);
	immVertex2f(pos, r->xmax, r->ymax);
	immVertex2f(pos, r->xmax, r->ymax);
	immVertex2f(pos, r->xmax - offsetx, r->ymax);

	immVertex2f(pos, r->xmin, r->ymax - offsety);
	immVertex2f(pos, r->xmin, r->ymax);
	immVertex2f(pos, r->xmin, r->ymax);
	immVertex2f(pos, r->xmin + offsetx, r->ymax);

	immEnd();

	immUnbindProgram();
}

static void rect_transform_draw_interaction(
        const float color[4], const int highlighted,
        const float size[2], const float margin[2],
        const float line_width)
{
	/* 4 verts for translate, otherwise only 3 are used. */
	float verts[4][2];
	uint verts_len = 0;

	switch (highlighted) {
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X:
			ARRAY_SET_ITEMS(verts[0], -size[0] + margin[0], -size[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0],             -size[1]);
			ARRAY_SET_ITEMS(verts[2], -size[0],              size[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X:
			ARRAY_SET_ITEMS(verts[0], size[0] - margin[0], -size[1]);
			ARRAY_SET_ITEMS(verts[1], size[0],             -size[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],              size[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y:
			ARRAY_SET_ITEMS(verts[0], -size[0], -size[1] + margin[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0], -size[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],  -size[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y:
			ARRAY_SET_ITEMS(verts[0], -size[0], size[1] - margin[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0], size[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],  size[1]);
			verts_len = 3;
			break;

		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y:
			ARRAY_SET_ITEMS(verts[0], -size[0] + margin[0], -size[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0] + margin[0], -size[1] + margin[1]);
			ARRAY_SET_ITEMS(verts[2], -size[0],             -size[1] + margin[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y:
			ARRAY_SET_ITEMS(verts[0], -size[0] + margin[0], size[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0] + margin[0], size[1] - margin[1]);
			ARRAY_SET_ITEMS(verts[2], -size[0],             size[1] - margin[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y:
			ARRAY_SET_ITEMS(verts[0], size[0] - margin[0], -size[1]);
			ARRAY_SET_ITEMS(verts[1], size[0] - margin[0], -size[1] + margin[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],             -size[1] + margin[1]);
			verts_len = 3;
			break;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y:
			ARRAY_SET_ITEMS(verts[0], size[0] - margin[0], size[1]);
			ARRAY_SET_ITEMS(verts[1], size[0] - margin[0], size[1] - margin[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],             size[1] - margin[1]);
			verts_len = 3;
			break;

		case ED_MANIPULATOR_CAGE2D_PART_ROTATE:
		{
			const float rotate_pt[2] = {0.0f, size[1] + margin[1]};
			const rctf r_rotate = {
				.xmin = rotate_pt[0] - margin[0] / 2.0f,
				.xmax = rotate_pt[0] + margin[0] / 2.0f,
				.ymin = rotate_pt[1] - margin[1] / 2.0f,
				.ymax = rotate_pt[1] + margin[1] / 2.0f,
			};

			ARRAY_SET_ITEMS(verts[0], r_rotate.xmin, r_rotate.ymin);
			ARRAY_SET_ITEMS(verts[1], r_rotate.xmin, r_rotate.ymax);
			ARRAY_SET_ITEMS(verts[2], r_rotate.xmax, r_rotate.ymax);
			ARRAY_SET_ITEMS(verts[3], r_rotate.xmax, r_rotate.ymin);
			verts_len = 4;
			break;
		}

		/* Only used for 3D view selection, never displayed to the user. */
		case ED_MANIPULATOR_CAGE2D_PART_TRANSLATE:
			ARRAY_SET_ITEMS(verts[0], -size[0], -size[1]);
			ARRAY_SET_ITEMS(verts[1], -size[0],  size[1]);
			ARRAY_SET_ITEMS(verts[2], size[0],   size[1]);
			ARRAY_SET_ITEMS(verts[3], size[0],  -size[1]);
			verts_len = 4;
			break;
		default:
			return;
	}

	Gwn_VertFormat *format = immVertexFormat();
	struct {
		uint pos, col;
	} attr_id = {
		.pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT),
		.col = GWN_vertformat_attr_add(format, "color", GWN_COMP_F32, 3, GWN_FETCH_FLOAT),
	};
	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

	if (highlighted == ED_MANIPULATOR_CAGE2D_PART_TRANSLATE) {
		immBegin(GWN_PRIM_TRI_FAN, 4);
		immAttrib3f(attr_id.col, 0.0f, 0.0f, 0.0f);
		for (uint i = 0; i < verts_len; i++) {
			immVertex2fv(attr_id.pos, verts[i]);
		}
		immEnd();
	}
	else {
		glLineWidth(line_width + 3.0f);

		immBegin(GWN_PRIM_LINE_STRIP, verts_len);
		immAttrib3f(attr_id.col, 0.0f, 0.0f, 0.0f);
		for (uint i = 0; i < verts_len; i++) {
			immVertex2fv(attr_id.pos, verts[i]);
		}
		immEnd();

		glLineWidth(line_width);

		immBegin(GWN_PRIM_LINE_STRIP, verts_len);
		immAttrib3fv(attr_id.col, color);
		for (uint i = 0; i < verts_len; i++) {
			immVertex2fv(attr_id.pos, verts[i]);
		}
		immEnd();
	}

	immUnbindProgram();

}

static void manipulator_rect_transform_draw_intern(
        wmManipulator *mpr, const bool select, const bool highlight, const int select_id)
{
	// const bool use_clamp = (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) == 0;
	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	const float w = dims[0] * (1.0f + (1.0f / 3.0f));
	const float h = dims[1] * (1.0f + (1.0f / 3.0f));
	float matrix_final[4][4];

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");

	float aspx = 1.0f, aspy = 1.0f;
	const float size[2] = {w / 2.0f, h / 2.0f};
	const rctf r = {
		.xmin = -size[0],
		.ymin = -size[1],
		.xmax = size[0],
		.ymax = size[1],
	};

	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	if (w > h) {
		aspx = h / w;
	}
	else {
		aspy = w / h;
	}

	const float margin[2] = {
		aspx * w / MANIPULATOR_RESIZER_WIDTH,
		aspy * h / MANIPULATOR_RESIZER_WIDTH,
	};

	/* corner manipulators */
	glLineWidth(mpr->line_width + 3.0f);
	rect_transform_draw_corners(&r, margin[0], margin[1], (const float[3]){0, 0, 0});

	if (false) {
		glEnable(GL_BLEND);
		uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformColor4fv((const float[4]){1, 1, 1, 0.5f});
		float s = 2.0f / 3.0f;
		immRectf(pos, -s, -s, s, s);
		immUnbindProgram();
		glDisable(GL_BLEND);
	}

	/* corner manipulators */
	{
		float color[4];
		manipulator_color_get(mpr, highlight, color);
		glLineWidth(mpr->line_width);
		rect_transform_draw_corners(&r, margin[0], margin[1], color);
	}

	if (select) {
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE) {
			int scale_parts[] = {
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y,

				ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y,
				ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y,
			};
			for (int i = 0; i < ARRAY_SIZE(scale_parts); i++) {
				GPU_select_load_id(select_id | scale_parts[i]);
				rect_transform_draw_interaction(mpr->color, scale_parts[i], size, margin, mpr->line_width);
			}
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
			const int transform_part = ED_MANIPULATOR_CAGE2D_PART_TRANSLATE;
			GPU_select_load_id(select_id | transform_part);
			rect_transform_draw_interaction(mpr->color, transform_part, size, margin, mpr->line_width);
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
			rect_transform_draw_interaction(
			        mpr->color, ED_MANIPULATOR_CAGE2D_PART_ROTATE, size, margin, mpr->line_width);
		}
	}
	else {
		/* Don't draw translate (only for selection). */
		if (mpr->highlight_part != ED_MANIPULATOR_CAGE2D_PART_TRANSLATE) {
			rect_transform_draw_interaction(mpr->color, mpr->highlight_part, size, margin, mpr->line_width);
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
			rect_transform_draw_interaction(
			        mpr->color, ED_MANIPULATOR_CAGE2D_PART_ROTATE, size, margin, mpr->line_width);
		}
	}

	glLineWidth(1.0);
	gpuPopMatrix();
}

/**
 * For when we want to draw 2d cage in 3d views.
 */
static void manipulator_rect_transform_draw_select(const bContext *UNUSED(C), wmManipulator *mpr, int select_id)
{
	manipulator_rect_transform_draw_intern(mpr, true, false, select_id);
}

static void manipulator_rect_transform_draw(const bContext *UNUSED(C), wmManipulator *mpr)
{
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;
	manipulator_rect_transform_draw_intern(mpr, false, is_highlight, -1);
}

static int manipulator_rect_transform_get_cursor(wmManipulator *mpr)
{
	int highlight_part = mpr->highlight_part;

	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		return BC_NSEW_SCROLLCURSOR;
	}

	switch (highlight_part) {
		case ED_MANIPULATOR_CAGE2D_PART_TRANSLATE:
			return BC_HANDCURSOR;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X:
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X:
			return CURSOR_X_MOVE;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y:
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y:
			return CURSOR_Y_MOVE;

			/* TODO diagonal cursor */
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y:
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y:
			return BC_NSEW_SCROLLCURSOR;
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y:
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y:
			return BC_NSEW_SCROLLCURSOR;
		case ED_MANIPULATOR_CAGE2D_PART_ROTATE:
			return BC_CROSSCURSOR;
		default:
			return CURSOR_STD;
	}
}

static int manipulator_rect_transform_test_select(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	//float matrot[2][2];
	float point_local[2];
	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	const float w = dims[0] * 2;
	const float h = dims[1] * 2;
	const float size[2] = {w / 2.0f, h / 2.0f};
	float aspx = 1.0f, aspy = 1.0f;

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, true, point_local) == false)
	{
		return 0;
	}

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	if (dims[0] > dims[1]) {
		aspx = h / w;
	}
	else {
		aspy = w / h;
	}

	const float margin[2] = {
		aspx * w / MANIPULATOR_RESIZER_WIDTH,
		aspy * h / MANIPULATOR_RESIZER_WIDTH,
	};

	rctf r;

	r.xmin = -size[0] + margin[0];
	r.ymin = -size[1] + margin[1];
	r.xmax = size[0] - margin[0];
	r.ymax = size[1] - margin[1];

	bool isect = BLI_rctf_isect_pt_v(&r, point_local);

	if (isect)
		return ED_MANIPULATOR_CAGE2D_PART_TRANSLATE;

	/* if manipulator does not have a scale intersection, don't do it */
	if (transform_flag &
	    (ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM))
	{
		const rctf r_xmin = {.xmin = -size[0],    .ymin = -size[1],    .xmax = -size[0] + margin[0], .ymax = size[1]};
		const rctf r_xmax = {.xmin = size[0] - margin[0], .ymin = -size[1],    .xmax = size[0],      .ymax = size[1]};
		const rctf r_ymin = {.xmin = -size[0],    .ymin = -size[1],    .xmax = size[0],      .ymax = -size[1] + margin[1]};
		const rctf r_ymax = {.xmin = -size[0],    .ymin = size[1] - margin[1], .xmax = size[0],      .ymax = size[1]};

		if (BLI_rctf_isect_pt_v(&r_xmin, point_local)) {
			if (BLI_rctf_isect_pt_v(&r_ymin, point_local)) {
				return ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y;
			}
			if (BLI_rctf_isect_pt_v(&r_ymax, point_local)) {
				return ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y;
			}
			return ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X;
		}
		if (BLI_rctf_isect_pt_v(&r_xmax, point_local)) {
			if (BLI_rctf_isect_pt_v(&r_ymin, point_local)) {
				return ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y;
			}
			if (BLI_rctf_isect_pt_v(&r_ymax, point_local)) {
				return ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y;
			}
			return ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X;
		}
		if (BLI_rctf_isect_pt_v(&r_ymin, point_local)) {
			return ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y;
		}
		if (BLI_rctf_isect_pt_v(&r_ymax, point_local)) {
			return ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y;
		}
	}

	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
		/* Rotate:
		 *  (*) <-- hot spot is here!
		 * +---+
		 * |   |
		 * +---+ */
		const float r_rotate_pt[2] = {0.0f, size[1] + margin[1]};
		const rctf r_rotate = {
			.xmin = r_rotate_pt[0] - margin[0] / 2.0f,
			.xmax = r_rotate_pt[0] + margin[0] / 2.0f,
			.ymin = r_rotate_pt[1] - margin[1] / 2.0f,
			.ymax = r_rotate_pt[1] + margin[1] / 2.0f,
		};

		if (BLI_rctf_isect_pt_v(&r_rotate, point_local)) {
			return ED_MANIPULATOR_CAGE2D_PART_ROTATE;
		}
	}

	return -1;
}

typedef struct RectTransformInteraction {
	float orig_mouse[2];
	float orig_matrix_offset[4][4];
} RectTransformInteraction;

static void manipulator_rect_transform_setup(wmManipulator *mpr)
{
	mpr->flag |= WM_MANIPULATOR_DRAW_MODAL;
}

static int manipulator_rect_transform_invoke(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	RectTransformInteraction *data = MEM_callocN(sizeof(RectTransformInteraction), "cage_interaction");

	copy_m4_m4(data->orig_matrix_offset, mpr->matrix_offset);

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, data->orig_mouse) == 0)
	{
		zero_v2(data->orig_mouse);
	}

	mpr->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static int manipulator_rect_transform_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	float point_local[2];

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, point_local) == false)
	{
		return OPERATOR_RUNNING_MODAL;
	}

	/* TODO! (currently a nop) */

	wmManipulatorProperty *mpr_prop;

	mpr_prop = WM_manipulator_target_property_find(mpr, "matrix");
	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, &mpr->matrix_offset[0][0]);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(CTX_wm_region(C));
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_rect_transform_property_update(wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
{
	if (STREQ(mpr_prop->type->idname, "matrix")) {
		if (WM_manipulator_target_property_array_length(mpr, mpr_prop) == 16) {
			WM_manipulator_target_property_value_get_array(mpr, mpr_prop, &mpr->matrix_offset[0][0]);
		}
		else {
			BLI_assert(0);
		}
	}
	else {
		BLI_assert(0);
	}
}

static void manipulator_rect_transform_exit(bContext *C, wmManipulator *mpr, const bool cancel)
{
	RectTransformInteraction *data = mpr->interaction_data;

	if (!cancel)
		return;

	wmManipulatorProperty *mpr_prop;

	/* reset properties */
	mpr_prop = WM_manipulator_target_property_find(mpr, "matrix");
	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, &data->orig_matrix_offset[0][0]);
	}

	copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
}


/* -------------------------------------------------------------------- */
/** \name Cage Manipulator API
 *
 * \{ */

static void MANIPULATOR_WT_cage_2d_rotate(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_cage_2d_rotate";

	/* api callbacks */
	wt->draw = manipulator_rect_transform_draw;
	wt->draw_select = manipulator_rect_transform_draw_select;
	wt->test_select = manipulator_rect_transform_test_select;
	wt->setup = manipulator_rect_transform_setup;
	wt->invoke = manipulator_rect_transform_invoke;
	wt->property_update = manipulator_rect_transform_property_update;
	wt->modal = manipulator_rect_transform_modal;
	wt->exit = manipulator_rect_transform_exit;
	wt->cursor_get = manipulator_rect_transform_get_cursor;

	wt->struct_size = sizeof(wmManipulator);

	/* rna */
	static EnumPropertyItem rna_enum_transform[] = {
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE, "ROTATE", 0, "Rotate", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE, "SCALE", 0, "Scale", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM, "SCALE_UNIFORM", 0, "Scale Uniform", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static float unit_v2[2] = {1.0f, 1.0f};
	RNA_def_float_vector(wt->srna, "dimensions", 2, unit_v2, 0, FLT_MAX, "Dimensions", "", 0.0f, FLT_MAX);
	RNA_def_enum_flag(wt->srna, "transform", rna_enum_transform, 0, "Transform Options", "");

	WM_manipulatortype_target_property_def(wt, "matrix", PROP_FLOAT, 16);
}

void ED_manipulatortypes_cage_2d_rotate(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_cage_2d_rotate);
}

/** \} */
