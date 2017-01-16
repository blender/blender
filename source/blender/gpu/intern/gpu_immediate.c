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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "UI_resources.h"
#include "BLI_utildefines.h"

#include "gpu_shader_private.h"

void immBindBuiltinProgram(GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	immBindProgram(shader->program);
}

void immUniformThemeColor(int color_id)
{
	float color[4];
	UI_GetThemeColor4fv(color_id, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorShade(int color_id, int offset)
{
	float color[4];
	UI_GetThemeColorShade4fv(color_id, offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorShadeAlpha(int color_id, int color_offset, int alpha_offset)
{
	float color[4];
	UI_GetThemeColorShadeAlpha4fv(color_id, color_offset, alpha_offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorBlendShade(int color_id1, int color_id2, float fac, int offset)
{
	float color[4];
	UI_GetThemeColorBlendShade4fv(color_id1, color_id2, fac, offset, color);
	immUniformColor4fv(color);
}

void immUniformThemeColorBlend(int color_id1, int color_id2, float fac)
{
	uint8_t color[3];
	UI_GetThemeColorBlend3ubv(color_id1, color_id2, fac, color);
	immUniformColor3ubv(color);
}

void immThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	unsigned char col[4];
	UI_GetThemeColorShadeAlpha4ubv(colorid, coloffset, alphaoffset, col);
	immUniformColor4ub(col[0], col[1], col[2], col[3]);
}
