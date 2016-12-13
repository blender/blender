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
 * 
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Immediate mode rendering is powered by the Gawain library.
 * This file contains any additions or modifications specific to Blender.
 */

#pragma once

#include "gawain/immediate.h"
#include "gawain/imm_util.h"
#include "GPU_shader.h"

/* Extend immBindProgram to use Blender’s library of built-in shader programs.
 * Use immUnbindProgram() when done. */
void immBindBuiltinProgram(GPUBuiltinShader shader_id);

/*
 * Extend immUniformColor to take Blender's themes
 */
void immUniformThemeColor(int color_id);
void immUniformThemeColorShade(int color_id, int offset);
void immUniformThemeColorShadeAlpha(int color_id, int color_offset, int alpha_offset);
void immUniformThemeColorBlendShade(int color_id1, int color_id2, float fac, int offset);
void immUniformThemeColorBlend(int color_id1, int color_id2, float fac);
