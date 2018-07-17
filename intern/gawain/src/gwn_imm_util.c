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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gwn_imm_util.c
 *  \ingroup gpu
 *
 * Gawain immediate mode drawing utilities
 */

#include "gwn_imm_util.h"
#include "gwn_immediate.h"
#include <stdlib.h>

void immRectf(uint pos, float x1, float y1, float x2, float y2)
{
	immBegin(GWN_PRIM_TRI_FAN, 4);
	immVertex2f(pos, x1, y1);
	immVertex2f(pos, x2, y1);
	immVertex2f(pos, x2, y2);
	immVertex2f(pos, x1, y2);
	immEnd();
}

void immRecti(uint pos, int x1, int y1, int x2, int y2)
{
	immBegin(GWN_PRIM_TRI_FAN, 4);
	immVertex2i(pos, x1, y1);
	immVertex2i(pos, x2, y1);
	immVertex2i(pos, x2, y2);
	immVertex2i(pos, x1, y2);
	immEnd();
}

void immRectf_fast_with_color(uint pos, uint col, float x1, float y1, float x2, float y2, const float color[4])
{
	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y2);

	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y2);
	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y2);
}

void immRecti_fast_with_color(uint pos, uint col, int x1, int y1, int x2, int y2, const float color[4])
{
	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y2);

	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y2);
	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y2);
}

#if 0 /* more complete version in case we want that */
void immRecti_complete(int x1, int y1, int x2, int y2, const float color[4])
{
	Gwn_VertFormat *format = immVertexFormat();
	uint pos = add_attrib(format, "pos", GWN_COMP_I32, 2, GWN_FETCH_INT_TO_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	immRecti(pos, x1, y1, x2, y2);
	immUnbindProgram();
}
#endif
