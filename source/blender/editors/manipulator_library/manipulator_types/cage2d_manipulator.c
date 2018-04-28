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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_dial_2d.h"
#include "BLI_rect.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_manipulator_library.h"

/* own includes */
#include "../manipulator_library_intern.h"

#define MANIPULATOR_RESIZER_SIZE 10.0f
#define MANIPULATOR_MARGIN_OFFSET_SCALE 1.5f

static void manipulator_calc_matrix_final_no_offset(
        const wmManipulator *mpr, float orig_matrix_final_no_offset[4][4])
{
	float mat_identity[4][4];
	struct WM_ManipulatorMatrixParams params = {NULL};
	unit_m4(mat_identity);
	params.matrix_offset = mat_identity;
	WM_manipulator_calc_matrix_final_params(mpr, &params, orig_matrix_final_no_offset);
}

static void manipulator_calc_rect_view_scale(
        const wmManipulator *mpr, const float dims[2], float scale[2])
{
	float matrix_final_no_offset[4][4];
	float asp[2] = {1.0f, 1.0f};
	if (dims[0] > dims[1]) {
		asp[0] = dims[1] / dims[0];
	}
	else {
		asp[1] = dims[0] / dims[1];
	}
	float x_axis[3], y_axis[3];
	manipulator_calc_matrix_final_no_offset(mpr, matrix_final_no_offset);
	mul_v3_mat3_m4v3(x_axis, matrix_final_no_offset, mpr->matrix_offset[0]);
	mul_v3_mat3_m4v3(y_axis, matrix_final_no_offset, mpr->matrix_offset[1]);

	mul_v2_v2(x_axis, asp);
	mul_v2_v2(y_axis, asp);

	scale[0] = 1.0f / len_v3(x_axis);
	scale[1] = 1.0f / len_v3(y_axis);
}

static void manipulator_calc_rect_view_margin(
        const wmManipulator *mpr, const float dims[2], float margin[2])
{
	float handle_size;
	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		handle_size = 0.15f;
	}
	else {
		handle_size = MANIPULATOR_RESIZER_SIZE;
	}
	handle_size *= mpr->scale_final;
	float scale_xy[2];
	manipulator_calc_rect_view_scale(mpr, dims, scale_xy);
	margin[0] = ((handle_size * scale_xy[0]));
	margin[1] = ((handle_size * scale_xy[1]));
}

/* -------------------------------------------------------------------- */

static void manipulator_rect_pivot_from_scale_part(int part, float r_pt[2], bool r_constrain_axis[2])
{
	bool x = true, y = true;
	switch (part) {
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X: { ARRAY_SET_ITEMS(r_pt,  0.5,  0.0); x = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X: { ARRAY_SET_ITEMS(r_pt, -0.5,  0.0); x = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y: { ARRAY_SET_ITEMS(r_pt,  0.0,  0.5); y = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y: { ARRAY_SET_ITEMS(r_pt,  0.0, -0.5); y = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y: { ARRAY_SET_ITEMS(r_pt,  0.5,  0.5); x = y = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y: { ARRAY_SET_ITEMS(r_pt,  0.5, -0.5); x = y = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y: { ARRAY_SET_ITEMS(r_pt, -0.5,  0.5); x = y = false; break; }
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y: { ARRAY_SET_ITEMS(r_pt, -0.5, -0.5); x = y = false; break; }
		default: BLI_assert(0);
	}
	r_constrain_axis[0] = x;
	r_constrain_axis[1] = y;
}

/* -------------------------------------------------------------------- */
/** \name Box Draw Style
 *
 * Useful for 3D views, see: #ED_MANIPULATOR_CAGE2D_STYLE_BOX
 * \{ */

static void cage2d_draw_box_corners(
        const rctf *r, const float margin[2], const float color[3])
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	immBegin(GWN_PRIM_LINES, 16);

	immVertex2f(pos, r->xmin, r->ymin + margin[1]);
	immVertex2f(pos, r->xmin, r->ymin);
	immVertex2f(pos, r->xmin, r->ymin);
	immVertex2f(pos, r->xmin + margin[0], r->ymin);

	immVertex2f(pos, r->xmax, r->ymin + margin[1]);
	immVertex2f(pos, r->xmax, r->ymin);
	immVertex2f(pos, r->xmax, r->ymin);
	immVertex2f(pos, r->xmax - margin[0], r->ymin);

	immVertex2f(pos, r->xmax, r->ymax - margin[1]);
	immVertex2f(pos, r->xmax, r->ymax);
	immVertex2f(pos, r->xmax, r->ymax);
	immVertex2f(pos, r->xmax - margin[0], r->ymax);

	immVertex2f(pos, r->xmin, r->ymax - margin[1]);
	immVertex2f(pos, r->xmin, r->ymax);
	immVertex2f(pos, r->xmin, r->ymax);
	immVertex2f(pos, r->xmin + margin[0], r->ymax);

	immEnd();

	immUnbindProgram();
}

static void cage2d_draw_box_interaction(
        const float color[4], const int highlighted,
        const float size[2], const float margin[2],
        const float line_width, const bool is_solid, const int draw_options)
{
	/* 4 verts for translate, otherwise only 3 are used. */
	float verts[4][2];
	uint verts_len = 0;
	Gwn_PrimType prim_type = GWN_PRIM_NONE;

	switch (highlighted) {
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X:
		{
			rctf r = {
				.xmin = -size[0], .xmax = -size[0] + margin[0],
				.ymin = -size[1] + margin[1], .ymax = size[1] - margin[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmin, r.ymin);
			ARRAY_SET_ITEMS(verts[1], r.xmin, r.ymax);
			verts_len = 2;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[2], r.xmax, r.ymax);
				ARRAY_SET_ITEMS(verts[3], r.xmax, r.ymin);
				verts_len += 2;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X:
		{
			rctf r = {
				.xmin = size[0] - margin[0], .xmax = size[0],
				.ymin = -size[1] + margin[1], .ymax = size[1] - margin[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmax, r.ymin);
			ARRAY_SET_ITEMS(verts[1], r.xmax, r.ymax);
			verts_len = 2;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[2], r.xmin, r.ymax);
				ARRAY_SET_ITEMS(verts[3], r.xmin, r.ymin);
				verts_len += 2;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_Y:
		{
			rctf r = {
				.xmin = -size[0] + margin[0], .xmax = size[0] - margin[0],
				.ymin = -size[1], .ymax = -size[1] + margin[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmin, r.ymin);
			ARRAY_SET_ITEMS(verts[1], r.xmax, r.ymin);
			verts_len = 2;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[2], r.xmax, r.ymax);
				ARRAY_SET_ITEMS(verts[3], r.xmin, r.ymax);
				verts_len += 2;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_Y:
		{
			rctf r = {
				.xmin = -size[0] + margin[0], .xmax = size[0] - margin[0],
				.ymin = size[1] - margin[1], .ymax = size[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmin, r.ymax);
			ARRAY_SET_ITEMS(verts[1], r.xmax, r.ymax);
			verts_len = 2;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[2], r.xmax, r.ymin);
				ARRAY_SET_ITEMS(verts[3], r.xmin, r.ymin);
				verts_len += 2;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MIN_Y:
		{
			rctf r = {
				.xmin = -size[0], .xmax = -size[0] + margin[0],
				.ymin = -size[1], .ymax = -size[1] + margin[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmax, r.ymin);
			ARRAY_SET_ITEMS(verts[1], r.xmax, r.ymax);
			ARRAY_SET_ITEMS(verts[2], r.xmin, r.ymax);
			verts_len = 3;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[3], r.xmin, r.ymin);
				verts_len += 1;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MIN_X_MAX_Y:
		{
			rctf r = {
				.xmin = -size[0], .xmax = -size[0] + margin[0],
				.ymin = size[1] - margin[1], .ymax = size[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmax, r.ymax);
			ARRAY_SET_ITEMS(verts[1], r.xmax, r.ymin);
			ARRAY_SET_ITEMS(verts[2], r.xmin, r.ymin);
			verts_len = 3;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[3], r.xmin, r.ymax);
				verts_len += 1;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MIN_Y:
		{
			rctf r = {
				.xmin = size[0] - margin[0], .xmax = size[0],
				.ymin = -size[1], .ymax = -size[1] + margin[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmin, r.ymin);
			ARRAY_SET_ITEMS(verts[1], r.xmin, r.ymax);
			ARRAY_SET_ITEMS(verts[2], r.xmax, r.ymax);
			verts_len = 3;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[3], r.xmax, r.ymin);
				verts_len += 1;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
		case ED_MANIPULATOR_CAGE2D_PART_SCALE_MAX_X_MAX_Y:
		{
			rctf r = {
				.xmin = size[0] - margin[0], .xmax = size[0],
				.ymin = size[1] - margin[1], .ymax = size[1],
			};
			ARRAY_SET_ITEMS(verts[0], r.xmin, r.ymax);
			ARRAY_SET_ITEMS(verts[1], r.xmin, r.ymin);
			ARRAY_SET_ITEMS(verts[2], r.xmax, r.ymin);
			verts_len = 3;
			if (is_solid) {
				ARRAY_SET_ITEMS(verts[3], r.xmax, r.ymax);
				verts_len += 1;
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}
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
			if (is_solid) {
				prim_type = GWN_PRIM_TRI_FAN;
			}
			else {
				prim_type = GWN_PRIM_LINE_STRIP;
			}
			break;
		}

		case ED_MANIPULATOR_CAGE2D_PART_TRANSLATE:
			if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
				ARRAY_SET_ITEMS(verts[0], -margin[0] / 2, -margin[1] / 2);
				ARRAY_SET_ITEMS(verts[1],  margin[0] / 2,  margin[1] / 2);
				ARRAY_SET_ITEMS(verts[2], -margin[0] / 2,  margin[1] / 2);
				ARRAY_SET_ITEMS(verts[3],  margin[0] / 2, -margin[1] / 2);
				verts_len = 4;
				if (is_solid) {
					prim_type = GWN_PRIM_TRI_FAN;
				}
				else {
					prim_type = GWN_PRIM_LINES;
				}
			}
			else {
				/* Only used for 3D view selection, never displayed to the user. */
				ARRAY_SET_ITEMS(verts[0], -size[0], -size[1]);
				ARRAY_SET_ITEMS(verts[1], -size[0],  size[1]);
				ARRAY_SET_ITEMS(verts[2], size[0],   size[1]);
				ARRAY_SET_ITEMS(verts[3], size[0],  -size[1]);
				verts_len = 4;
				if (is_solid) {
					prim_type = GWN_PRIM_TRI_FAN;
				}
				else {
					/* unreachable */
					BLI_assert(0);
					prim_type = GWN_PRIM_LINE_STRIP;
				}
			}
			break;
		default:
			return;
	}

	BLI_assert(prim_type != GWN_PRIM_NONE);

	Gwn_VertFormat *format = immVertexFormat();
	struct {
		uint pos, col;
	} attr_id = {
		.pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT),
		.col = GWN_vertformat_attr_add(format, "color", GWN_COMP_F32, 3, GWN_FETCH_FLOAT),
	};
	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

	{
		if (is_solid) {
			BLI_assert(ELEM(prim_type, GWN_PRIM_TRI_FAN));
			immBegin(prim_type, verts_len);
			immAttrib3f(attr_id.col, 0.0f, 0.0f, 0.0f);
			for (uint i = 0; i < verts_len; i++) {
				immVertex2fv(attr_id.pos, verts[i]);
			}
			immEnd();
		}
		else {
			BLI_assert(ELEM(prim_type, GWN_PRIM_LINE_STRIP, GWN_PRIM_LINES));
			glLineWidth(line_width + 3.0f);

			immBegin(prim_type, verts_len);
			immAttrib3f(attr_id.col, 0.0f, 0.0f, 0.0f);
			for (uint i = 0; i < verts_len; i++) {
				immVertex2fv(attr_id.pos, verts[i]);
			}
			immEnd();

			glLineWidth(line_width);

			immBegin(prim_type, verts_len);
			immAttrib3fv(attr_id.col, color);
			for (uint i = 0; i < verts_len; i++) {
				immVertex2fv(attr_id.pos, verts[i]);
			}
			immEnd();
		}
	}

	immUnbindProgram();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Draw Style
 *
 * Useful for 2D views, see: #ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE
 * \{ */

static void imm_draw_point_aspect_2d(
        uint pos, float x, float y, float rad_x, float rad_y, bool solid)
{
	immBegin(solid ? GWN_PRIM_TRI_FAN : GWN_PRIM_LINE_LOOP, 4);
	immVertex2f(pos, x - rad_x, y - rad_y);
	immVertex2f(pos, x - rad_x, y + rad_y);
	immVertex2f(pos, x + rad_x, y + rad_y);
	immVertex2f(pos, x + rad_x, y - rad_y);
	immEnd();
}

static void cage2d_draw_circle_wire(
        const rctf *r, const float margin[2], const float color[3],
        const int transform_flag, const int draw_options)
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	immBegin(GWN_PRIM_LINE_LOOP, 4);
	immVertex2f(pos, r->xmin, r->ymin);
	immVertex2f(pos, r->xmax, r->ymin);
	immVertex2f(pos, r->xmax, r->ymax);
	immVertex2f(pos, r->xmin, r->ymax);
	immEnd();

	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
		immBegin(GWN_PRIM_LINE_LOOP, 2);
		immVertex2f(pos, BLI_rctf_cent_x(r), r->ymax);
		immVertex2f(pos, BLI_rctf_cent_x(r), r->ymax + margin[1]);
		immEnd();
	}

	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
		if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
			const float rad[2] = {margin[0] / 2, margin[1] / 2};
			const float center[2] = {BLI_rctf_cent_x(r), BLI_rctf_cent_y(r)};

			immBegin(GWN_PRIM_LINES, 4);
			immVertex2f(pos, center[0] - rad[0], center[1] - rad[1]);
			immVertex2f(pos, center[0] + rad[0], center[1] + rad[1]);
			immVertex2f(pos, center[0] + rad[0], center[1] - rad[1]);
			immVertex2f(pos, center[0] - rad[0], center[1] + rad[1]);
			immEnd();
		}
	}

	immUnbindProgram();
}

static void cage2d_draw_circle_handles(
        const rctf *r, const float margin[2], const float color[3],
        const int transform_flag,
        bool solid)
{
	uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	void (*circle_fn)(uint, float, float, float, float, int) =
	        (solid) ? imm_draw_circle_fill_aspect_2d : imm_draw_circle_wire_aspect_2d;
	const int resolu = 12;
	const float rad[2] = {margin[0] / 3, margin[1] / 3};

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor3fv(color);

	/* should  really divide by two, but looks too bulky. */
	{
		imm_draw_point_aspect_2d(pos, r->xmin, r->ymin, rad[0], rad[1], solid);
		imm_draw_point_aspect_2d(pos, r->xmax, r->ymin, rad[0], rad[1], solid);
		imm_draw_point_aspect_2d(pos, r->xmax, r->ymax, rad[0], rad[1], solid);
		imm_draw_point_aspect_2d(pos, r->xmin, r->ymax, rad[0], rad[1], solid);
	}

	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
		const float handle[2] = {BLI_rctf_cent_x(r), r->ymax + (margin[1] * MANIPULATOR_MARGIN_OFFSET_SCALE)};
		circle_fn(pos, handle[0], handle[1], rad[0], rad[1], resolu);
	}

	immUnbindProgram();
}

/** \} */

static void manipulator_cage2d_draw_intern(
        wmManipulator *mpr, const bool select, const bool highlight, const int select_id)
{
	// const bool use_clamp = (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) == 0;
	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	float matrix_final[4][4];

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	const int draw_style = RNA_enum_get(mpr->ptr, "draw_style");
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");

	const float size_real[2] = {dims[0] / 2.0f, dims[1] / 2.0f};

	WM_manipulator_calc_matrix_final(mpr, matrix_final);

	gpuPushMatrix();
	gpuMultMatrix(matrix_final);

	float margin[2];
	manipulator_calc_rect_view_margin(mpr, dims, margin);

	/* Handy for quick testing draw (if it's outside bounds). */
	if (false) {
		glEnable(GL_BLEND);
		uint pos = GWN_vertformat_attr_add(immVertexFormat(), "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformColor4fv((const float[4]){1, 1, 1, 0.5f});
		float s = 0.5f;
		immRectf(pos, -s, -s, s, s);
		immUnbindProgram();
		glDisable(GL_BLEND);
	}

	if (select) {
		/* expand for hotspot */
		const float size[2] = {size_real[0] + margin[0] / 2, size_real[1] + margin[1] / 2};

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
				cage2d_draw_box_interaction(
				        mpr->color, scale_parts[i], size, margin, mpr->line_width, true, draw_options);
			}
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
			const int transform_part = ED_MANIPULATOR_CAGE2D_PART_TRANSLATE;
			GPU_select_load_id(select_id | transform_part);
			cage2d_draw_box_interaction(
			        mpr->color, transform_part, size, margin, mpr->line_width, true, draw_options);
		}
		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
			cage2d_draw_box_interaction(
			        mpr->color, ED_MANIPULATOR_CAGE2D_PART_ROTATE, size_real, margin, mpr->line_width, true, draw_options);
		}
	}
	else {
		const rctf r = {
			.xmin = -size_real[0],
			.ymin = -size_real[1],
			.xmax = size_real[0],
			.ymax = size_real[1],
		};
		if (draw_style == ED_MANIPULATOR_CAGE2D_STYLE_BOX) {
			/* corner manipulators */
			glLineWidth(mpr->line_width + 3.0f);
			cage2d_draw_box_corners(&r, margin, (const float[3]){0, 0, 0});

			/* corner manipulators */
			float color[4];
			manipulator_color_get(mpr, highlight, color);
			glLineWidth(mpr->line_width);
			cage2d_draw_box_corners(&r, margin, color);

			bool show = false;
			if (mpr->highlight_part == ED_MANIPULATOR_CAGE2D_PART_TRANSLATE) {
				/* Only show if we're drawing the center handle
				 * otherwise the entire rectangle is the hotspot. */
				if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
					show = true;
				}
			}
			else {
				show = true;
			}

			if (show) {
				cage2d_draw_box_interaction(
				        mpr->color, mpr->highlight_part, size_real, margin, mpr->line_width, false, draw_options);
			}

			if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE) {
				cage2d_draw_box_interaction(
				        mpr->color, ED_MANIPULATOR_CAGE2D_PART_ROTATE, size_real, margin, mpr->line_width, false, draw_options);
			}
		}
		else if (draw_style == ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE) {
			float color[4];
			manipulator_color_get(mpr, highlight, color);

			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_BLEND);

			glLineWidth(mpr->line_width + 3.0f);
			cage2d_draw_circle_wire(&r, margin, (const float[3]){0, 0, 0}, transform_flag, draw_options);
			glLineWidth(mpr->line_width);
			cage2d_draw_circle_wire(&r, margin, color, transform_flag, draw_options);


			/* corner manipulators */
			cage2d_draw_circle_handles(&r, margin, color, transform_flag, true);
			cage2d_draw_circle_handles(&r, margin, (const float[3]){0, 0, 0}, transform_flag, false);

			glDisable(GL_BLEND);
			glDisable(GL_LINE_SMOOTH);
		}
		else {
			BLI_assert(0);
		}
	}

	glLineWidth(1.0);
	gpuPopMatrix();
}

/**
 * For when we want to draw 2d cage in 3d views.
 */
static void manipulator_cage2d_draw_select(const bContext *UNUSED(C), wmManipulator *mpr, int select_id)
{
	manipulator_cage2d_draw_intern(mpr, true, false, select_id);
}

static void manipulator_cage2d_draw(const bContext *UNUSED(C), wmManipulator *mpr)
{
	const bool is_highlight = (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;
	manipulator_cage2d_draw_intern(mpr, false, is_highlight, -1);
}

static int manipulator_cage2d_get_cursor(wmManipulator *mpr)
{
	int highlight_part = mpr->highlight_part;

	if (mpr->parent_mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) {
		return BC_NSEW_SCROLLCURSOR;
	}

	switch (highlight_part) {
		case ED_MANIPULATOR_CAGE2D_PART_TRANSLATE:
			return BC_NSEW_SCROLLCURSOR;
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

static int manipulator_cage2d_test_select(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	float point_local[2];
	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	const float size_real[2] = {dims[0] / 2.0f, dims[1] / 2.0f};

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, true, point_local) == false)
	{
		return -1;
	}

	float margin[2];
	manipulator_calc_rect_view_margin(mpr, dims, margin);
	/* expand for hotspot */
	const float size[2] = {size_real[0] + margin[0] / 2, size_real[1] + margin[1] / 2};

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	const int draw_options = RNA_enum_get(mpr->ptr, "draw_options");

	if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
		rctf r;
		if (draw_options & ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE) {
			r.xmin = -margin[0] / 2;
			r.ymin = -margin[1] / 2;
			r.xmax =  margin[0] / 2;
			r.ymax =  margin[1] / 2;
		}
		else {
			r.xmin = -size[0] + margin[0];
			r.ymin = -size[1] + margin[1];
			r.xmax =  size[0] - margin[0];
			r.ymax =  size[1] - margin[1];
		};
		bool isect = BLI_rctf_isect_pt_v(&r, point_local);
		if (isect) {
			return ED_MANIPULATOR_CAGE2D_PART_TRANSLATE;
		}
	}

	/* if manipulator does not have a scale intersection, don't do it */
	if (transform_flag & (ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM)) {
		const rctf r_xmin = {.xmin = -size[0], .ymin = -size[1], .xmax = -size[0] + margin[0], .ymax = size[1]};
		const rctf r_xmax = {.xmin = size[0] - margin[0], .ymin = -size[1], .xmax = size[0], .ymax = size[1]};
		const rctf r_ymin = {.xmin = -size[0], .ymin = -size[1], .xmax = size[0], .ymax = -size[1] + margin[1]};
		const rctf r_ymax = {.xmin = -size[0], .ymin = size[1] - margin[1], .xmax = size[0], .ymax = size[1]};

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
		const float r_rotate_pt[2] = {0.0f, size_real[1] + (margin[1] * MANIPULATOR_MARGIN_OFFSET_SCALE)};
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
	float orig_matrix_final_no_offset[4][4];
	Dial *dial;
} RectTransformInteraction;

static void manipulator_cage2d_setup(wmManipulator *mpr)
{
	mpr->flag |= WM_MANIPULATOR_DRAW_MODAL | WM_MANIPULATOR_DRAW_NO_SCALE;
}

static int manipulator_cage2d_invoke(
        bContext *C, wmManipulator *mpr, const wmEvent *event)
{
	RectTransformInteraction *data = MEM_callocN(sizeof(RectTransformInteraction), "cage_interaction");

	copy_m4_m4(data->orig_matrix_offset, mpr->matrix_offset);
	manipulator_calc_matrix_final_no_offset(mpr, data->orig_matrix_final_no_offset);

	if (manipulator_window_project_2d(
	        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, data->orig_mouse) == 0)
	{
		zero_v2(data->orig_mouse);
	}

	mpr->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static int manipulator_cage2d_modal(
        bContext *C, wmManipulator *mpr, const wmEvent *event,
        eWM_ManipulatorTweak UNUSED(tweak_flag))
{
	/* For transform logic to be managable we operate in -0.5..0.5 2D space,
	 * no matter the size of the rectangle, mouse coorts are scaled to unit space.
	 * The mouse coords have been projected into the matrix so we don't need to worry about axis alignment.
	 *
	 * - The cursor offset are multiplied by 'dims'.
	 * - Matrix translation is also multiplied by 'dims'.
	 */
	RectTransformInteraction *data = mpr->interaction_data;
	float point_local[2];

	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);

	{
		float matrix_back[4][4];
		copy_m4_m4(matrix_back, mpr->matrix_offset);
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);

		bool ok = manipulator_window_project_2d(
		        C, mpr, (const float[2]){UNPACK2(event->mval)}, 2, false, point_local);
		copy_m4_m4(mpr->matrix_offset, matrix_back);
		if (!ok) {
			return OPERATOR_RUNNING_MODAL;
		}
	}

	const int transform_flag = RNA_enum_get(mpr->ptr, "transform");
	wmManipulatorProperty *mpr_prop;

	mpr_prop = WM_manipulator_target_property_find(mpr, "matrix");
	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_get_array(mpr, mpr_prop, &mpr->matrix_offset[0][0]);
	}

	if (mpr->highlight_part == ED_MANIPULATOR_CAGE2D_PART_TRANSLATE) {
		/* do this to prevent clamping from changing size */
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
		mpr->matrix_offset[3][0] = data->orig_matrix_offset[3][0] + (point_local[0] - data->orig_mouse[0]);
		mpr->matrix_offset[3][1] = data->orig_matrix_offset[3][1] + (point_local[1] - data->orig_mouse[1]);
	}
	else if (mpr->highlight_part == ED_MANIPULATOR_CAGE2D_PART_ROTATE) {

#define MUL_V2_V3_M4_FINAL(test_co, mouse_co) \
		mul_v3_m4v3(test_co, data->orig_matrix_final_no_offset, ((const float[3]){UNPACK2(mouse_co), 0.0}))

		float test_co[3];

		if (data->dial == NULL) {
			MUL_V2_V3_M4_FINAL(test_co, data->orig_matrix_offset[3]);

			data->dial = BLI_dial_initialize(test_co, FLT_EPSILON);

			MUL_V2_V3_M4_FINAL(test_co, data->orig_mouse);
			BLI_dial_angle(data->dial, test_co);
		}

		/* rotate */
		MUL_V2_V3_M4_FINAL(test_co, point_local);
		const float angle =  BLI_dial_angle(data->dial, test_co);

		float matrix_space_inv[4][4];
		float matrix_rotate[4][4];
		float pivot[3];

		copy_v3_v3(pivot, data->orig_matrix_offset[3]);

		invert_m4_m4(matrix_space_inv, mpr->matrix_space);

		unit_m4(matrix_rotate);
		mul_m4_m4m4(matrix_rotate, matrix_rotate, matrix_space_inv);
		rotate_m4(matrix_rotate, 'Z', -angle);
		mul_m4_m4m4(matrix_rotate, matrix_rotate, mpr->matrix_space);

		zero_v3(matrix_rotate[3]);
		transform_pivot_set_m4(matrix_rotate, pivot);

		mul_m4_m4m4(mpr->matrix_offset, matrix_rotate, data->orig_matrix_offset);

#undef MUL_V2_V3_M4_FINAL
	}
	else {
		/* scale */
		copy_m4_m4(mpr->matrix_offset, data->orig_matrix_offset);
		float pivot[2];
		bool constrain_axis[2] = {false};

		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE) {
			manipulator_rect_pivot_from_scale_part(mpr->highlight_part, pivot, constrain_axis);
		}
		else {
			zero_v2(pivot);
		}

		/* Cursor deltas scaled to (-0.5..0.5). */
		float delta_orig[2], delta_curr[2];
		for (int i = 0; i < 2; i++) {
			delta_orig[i] = ((data->orig_mouse[i] - data->orig_matrix_offset[3][i]) / dims[i]) - pivot[i];
			delta_curr[i] = ((point_local[i]      - data->orig_matrix_offset[3][i]) / dims[i]) - pivot[i];
		}

		float scale[2] = {1.0f, 1.0f};
		for (int i = 0; i < 2; i++) {
			if (constrain_axis[i] == false) {
				if (delta_orig[i] < 0.0f) {
					delta_orig[i] *= -1.0f;
					delta_curr[i] *= -1.0f;
				}
				const int sign = signum_i(scale[i]);

				scale[i] = 1.0f + ((delta_curr[i] - delta_orig[i]) / len_v3(data->orig_matrix_offset[i]));

				if ((transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_SIGNED) == 0) {
					if (sign != signum_i(scale[i])) {
						scale[i] = 0.0f;
					}
				}
			}
		}

		if (transform_flag & ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM) {
			if (constrain_axis[0] == false && constrain_axis[1] == false) {
				scale[1] = scale[0] = (scale[1] + scale[0]) / 2.0f;
			}
			else if (constrain_axis[0] == false) {
				scale[1] = scale[0];
			}
			else if (constrain_axis[1] == false) {
				scale[0] = scale[1];
			}
			else {
				BLI_assert(0);
			}
		}

		/* scale around pivot */
		float matrix_scale[4][4];
		unit_m4(matrix_scale);

		mul_v3_fl(matrix_scale[0], scale[0]);
		mul_v3_fl(matrix_scale[1], scale[1]);

		transform_pivot_set_m4(matrix_scale, (const float [3]){pivot[0] * dims[0], pivot[1] * dims[1], 0.0f});
		mul_m4_m4m4(mpr->matrix_offset, data->orig_matrix_offset, matrix_scale);
	}

	if (mpr_prop->type != NULL) {
		WM_manipulator_target_property_value_set_array(C, mpr, mpr_prop, &mpr->matrix_offset[0][0]);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(CTX_wm_region(C));
	WM_event_add_mousemove(C);

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_cage2d_property_update(wmManipulator *mpr, wmManipulatorProperty *mpr_prop)
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

static void manipulator_cage2d_exit(bContext *C, wmManipulator *mpr, const bool cancel)
{
	RectTransformInteraction *data = mpr->interaction_data;

	MEM_SAFE_FREE(data->dial);

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

static void MANIPULATOR_WT_cage_2d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_cage_2d";

	/* api callbacks */
	wt->draw = manipulator_cage2d_draw;
	wt->draw_select = manipulator_cage2d_draw_select;
	wt->test_select = manipulator_cage2d_test_select;
	wt->setup = manipulator_cage2d_setup;
	wt->invoke = manipulator_cage2d_invoke;
	wt->property_update = manipulator_cage2d_property_update;
	wt->modal = manipulator_cage2d_modal;
	wt->exit = manipulator_cage2d_exit;
	wt->cursor_get = manipulator_cage2d_get_cursor;

	wt->struct_size = sizeof(wmManipulator);

	/* rna */
	static EnumPropertyItem rna_enum_draw_style[] = {
		{ED_MANIPULATOR_CAGE2D_STYLE_BOX, "BOX", 0, "Box", ""},
		{ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE, "CIRCLE", 0, "Circle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_transform[] = {
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_ROTATE, "ROTATE", 0, "Rotate", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE, "SCALE", 0, "Scale", ""},
		{ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM, "SCALE_UNIFORM", 0, "Scale Uniform", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem rna_enum_draw_options[] = {
		{ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "XFORM_CENTER_HANDLE", 0, "Center Handle", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static float unit_v2[2] = {1.0f, 1.0f};
	RNA_def_float_vector(wt->srna, "dimensions", 2, unit_v2, 0, FLT_MAX, "Dimensions", "", 0.0f, FLT_MAX);
	RNA_def_enum_flag(wt->srna, "transform", rna_enum_transform, 0, "Transform Options", "");
	RNA_def_enum(wt->srna, "draw_style", rna_enum_draw_style, ED_MANIPULATOR_CAGE2D_STYLE_CIRCLE, "Draw Style", "");
	RNA_def_enum_flag(
	        wt->srna, "draw_options", rna_enum_draw_options,
	        ED_MANIPULATOR_CAGE2D_DRAW_FLAG_XFORM_CENTER_HANDLE, "Draw Options", "");

	WM_manipulatortype_target_property_def(wt, "matrix", PROP_FLOAT, 16);
}

void ED_manipulatortypes_cage_2d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_cage_2d);
}

/** \} */
