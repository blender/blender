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

/** \file GPU_state.h
 *  \ingroup gpu
 */

#ifndef __GPU_STATE_H__
#define __GPU_STATE_H__

/* These map directly to the GL_ blend functions, to minimize API add as needed*/
typedef enum GPUBlendFunction {
	GPU_ONE,
	GPU_SRC_ALPHA,
	GPU_ONE_MINUS_SRC_ALPHA,
	GPU_DST_COLOR,
	GPU_ZERO,
} GPUBlendFunction;

/* These map directly to the GL_ filter functions, to minimize API add as needed*/
typedef enum GPUFilterFunction {
	GPU_NEAREST,
	GPU_LINEAR
} GPUFilterFunction;

void GPU_blend(bool enable);
void GPU_blend_set_func(GPUBlendFunction sfactor, GPUBlendFunction dfactor);
void GPU_blend_set_func_separate(
        GPUBlendFunction src_rgb, GPUBlendFunction dst_rgb,
        GPUBlendFunction src_alpha, GPUBlendFunction dst_alpha);
void GPU_depth_test(bool enable);
bool GPU_depth_test_enabled(void);
void GPU_line_smooth(bool enable);
void GPU_line_stipple(bool enable);
void GPU_line_width(float width);
void GPU_point_size(float size);
void GPU_polygon_smooth(bool enable);
void GPU_scissor(int x, int y, int width, int height);
void GPU_scissor_get_f(float coords[4]);
void GPU_scissor_get_i(int coords[4]);
void GPU_viewport_size_get_f(float coords[4]);
void GPU_viewport_size_get_i(int coords[4]);

#endif  /* __GPU_STATE_H__ */
