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
 * Contributor(s): Ray Molenkamp
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_state.c
 *  \ingroup gpu
 *
 */

#include "GPU_glew.h"
#include "GPU_state.h"

static GLenum gpu_get_gl_blendfunction(GPUBlendFunction blend)
{
	switch (blend) {
		case GPU_ONE:
			return GL_ONE;
		case GPU_SRC_ALPHA:
			return GL_SRC_ALPHA;
		case GPU_ONE_MINUS_SRC_ALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;
		case GPU_DST_COLOR:
			return GL_DST_COLOR;
		case GPU_ZERO:
			return GL_ZERO;
		default:
			BLI_assert(!"Unhandled blend mode");
			return GL_ZERO;
	}
}

void GPU_blend(bool enable)
{
	if (enable) {
		glEnable(GL_BLEND);
	}
	else {
		glDisable(GL_BLEND);
	}
}

void GPU_blend_set_func(GPUBlendFunction sfactor, GPUBlendFunction dfactor)
{
	glBlendFunc(gpu_get_gl_blendfunction(sfactor), gpu_get_gl_blendfunction(dfactor));
}

void GPU_blend_set_func_separate(
	GPUBlendFunction src_rgb, GPUBlendFunction dst_rgb,
	GPUBlendFunction src_alpha, GPUBlendFunction dst_alpha)
{
	glBlendFuncSeparate(
	        gpu_get_gl_blendfunction(src_rgb),
	        gpu_get_gl_blendfunction(dst_rgb),
	        gpu_get_gl_blendfunction(src_alpha),
	        gpu_get_gl_blendfunction(dst_alpha));
}

void GPU_depth_test(bool enable)
{
	if (enable) {
		glEnable(GL_DEPTH_TEST);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}
}

bool GPU_depth_test_enabled()
{
	return glIsEnabled(GL_DEPTH_TEST);
}

void GPU_line_smooth(bool enable)
{
	if (enable) {
		glEnable(GL_LINE_SMOOTH);
	}
	else {
		glDisable(GL_LINE_SMOOTH);
	}
}

void GPU_line_stipple(bool enable)
{
	if (enable) {
		glEnable(GL_LINE_STIPPLE);
	}
	else {
		glDisable(GL_LINE_STIPPLE);
	}
}

void GPU_line_width(float width)
{
	glLineWidth(width);
}

void GPU_point_size(float size)
{
	glPointSize(size);
}

void GPU_polygon_smooth(bool enable)
{
	if (enable) {
		glEnable(GL_POLYGON_SMOOTH);
	}
	else {
		glDisable(GL_POLYGON_SMOOTH);
	}
}

void GPU_scissor(int x, int y, int width, int height)
{
	glScissor(x, y, width, height);
}

void GPU_scissor_get_f(float coords[4])
{
	glGetFloatv(GL_SCISSOR_BOX, coords);
}

void GPU_scissor_get_i(int coords[4])
{
	glGetIntegerv(GL_SCISSOR_BOX, coords);
}

void GPU_viewport_size_get_f(float coords[4])
{
	glGetFloatv(GL_VIEWPORT, coords);
}

void GPU_viewport_size_get_i(int coords[4])
{
	glGetIntegerv(GL_VIEWPORT, coords);
}
