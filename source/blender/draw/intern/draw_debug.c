/*
 * Copyright 2018, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_debug.c
 *  \ingroup draw
 *
 * \brief Simple API to draw debug shapes in the viewport.
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_link_utils.h"

#include "GPU_immediate.h"

#include "draw_debug.h"
#include "draw_manager.h"

/* --------- Register --------- */

/* Matrix applied to all points before drawing. Could be a stack if needed. */
static float g_modelmat[4][4];

void DRW_debug_modelmat_reset(void)
{
	unit_m4(g_modelmat);
}

void DRW_debug_modelmat(const float modelmat[4][4])
{
	copy_m4_m4(g_modelmat, modelmat);
}

void DRW_debug_line_v3v3(const float v1[3], const float v2[3], const float color[4])
{
	DRWDebugLine *line = MEM_mallocN(sizeof(DRWDebugLine), "DRWDebugLine");
	mul_v3_m4v3(line->pos[0], g_modelmat, v1);
	mul_v3_m4v3(line->pos[1], g_modelmat, v2);
	copy_v4_v4(line->color, color);
	BLI_LINKS_PREPEND(DST.debug.lines, line);
}

void DRW_debug_polygon_v3(const float (*v)[3], const int vert_len, const float color[4])
{
	BLI_assert(vert_len > 1);

	for (int i = 0; i < vert_len; ++i) {
		DRW_debug_line_v3v3(v[i], v[(i + 1) % vert_len], color);
	}
}

/* NOTE: g_modelmat is still applied on top. */
void DRW_debug_m4(const float m[4][4])
{
	float v0[3] = {0.0f, 0.0f, 0.0f};
	float v1[3] = {1.0f, 0.0f, 0.0f};
	float v2[3] = {0.0f, 1.0f, 0.0f};
	float v3[3] = {0.0f, 0.0f, 1.0f};

	mul_m4_v3(m, v0);
	mul_m4_v3(m, v1);
	mul_m4_v3(m, v2);
	mul_m4_v3(m, v3);

	DRW_debug_line_v3v3(v0, v1, (float[4]){1.0f, 0.0f, 0.0f, 1.0f});
	DRW_debug_line_v3v3(v0, v2, (float[4]){0.0f, 1.0f, 0.0f, 1.0f});
	DRW_debug_line_v3v3(v0, v3, (float[4]){0.0f, 0.0f, 1.0f, 1.0f});
}

void DRW_debug_bbox(const BoundBox *bbox, const float color[4])
{
	DRW_debug_line_v3v3(bbox->vec[0], bbox->vec[1], color);
	DRW_debug_line_v3v3(bbox->vec[1], bbox->vec[2], color);
	DRW_debug_line_v3v3(bbox->vec[2], bbox->vec[3], color);
	DRW_debug_line_v3v3(bbox->vec[3], bbox->vec[0], color);

	DRW_debug_line_v3v3(bbox->vec[4], bbox->vec[5], color);
	DRW_debug_line_v3v3(bbox->vec[5], bbox->vec[6], color);
	DRW_debug_line_v3v3(bbox->vec[6], bbox->vec[7], color);
	DRW_debug_line_v3v3(bbox->vec[7], bbox->vec[4], color);

	DRW_debug_line_v3v3(bbox->vec[0], bbox->vec[4], color);
	DRW_debug_line_v3v3(bbox->vec[1], bbox->vec[5], color);
	DRW_debug_line_v3v3(bbox->vec[2], bbox->vec[6], color);
	DRW_debug_line_v3v3(bbox->vec[3], bbox->vec[7], color);
}

/* --------- Render --------- */

static void drw_debug_draw_lines(void)
{
	int count = BLI_linklist_count((LinkNode *)DST.debug.lines);

	if (count == 0) {
		return;
	}

	Gwn_VertFormat *vert_format = immVertexFormat();
	uint pos = GWN_vertformat_attr_add(vert_format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	uint col = GWN_vertformat_attr_add(vert_format, "color", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	immBegin(GWN_PRIM_LINES, count * 2);

	while (DST.debug.lines) {
		void *next = DST.debug.lines->next;

		immAttrib4fv(col, DST.debug.lines->color);
		immVertex3fv(pos, DST.debug.lines->pos[0]);

		immAttrib4fv(col, DST.debug.lines->color);
		immVertex3fv(pos, DST.debug.lines->pos[1]);

		MEM_freeN(DST.debug.lines);
		DST.debug.lines = next;
	}
	immEnd();

	immUnbindProgram();
}

void drw_debug_draw(void)
{
	drw_debug_draw_lines();
}

void drw_debug_init(void)
{
	DRW_debug_modelmat_reset();
}
