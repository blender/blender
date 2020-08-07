/*
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
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* These map directly to the GL_ blend functions, to minimize API add as needed*/
typedef enum eGPUBlendFunction {
  GPU_ONE,
  GPU_SRC_ALPHA,
  GPU_ONE_MINUS_SRC_ALPHA,
  GPU_DST_COLOR,
  GPU_ZERO,
} eGPUBlendFunction;

/* These map directly to the GL_ filter functions, to minimize API add as needed*/
typedef enum eGPUFilterFunction {
  GPU_NEAREST,
  GPU_LINEAR,
} eGPUFilterFunction;

typedef enum eGPUFaceCull {
  GPU_CULL_NONE = 0, /* Culling disabled. */
  GPU_CULL_FRONT,
  GPU_CULL_BACK,
} eGPUFaceCull;

typedef enum eGPUProvokingVertex {
  GPU_VERTEX_FIRST = 0,
  GPU_VERTEX_LAST, /* Default */
} eGPUProvokingVertex;

/* Initialize
 * - sets the default Blender opengl state, if in doubt, check
 *   the contents of this function
 * - this is called when starting Blender, for opengl rendering. */
void GPU_state_init(void);

void GPU_blend(bool enable);
void GPU_blend_set_func(eGPUBlendFunction sfactor, eGPUBlendFunction dfactor);
void GPU_blend_set_func_separate(eGPUBlendFunction src_rgb,
                                 eGPUBlendFunction dst_rgb,
                                 eGPUBlendFunction src_alpha,
                                 eGPUBlendFunction dst_alpha);
void GPU_face_culling(eGPUFaceCull culling);
void GPU_front_facing(bool invert);
void GPU_provoking_vertex(eGPUProvokingVertex vert);
void GPU_depth_range(float near, float far);
void GPU_depth_test(bool enable);
bool GPU_depth_test_enabled(void);
void GPU_scissor_test(bool enable);
void GPU_line_smooth(bool enable);
void GPU_line_width(float width);
void GPU_point_size(float size);
void GPU_polygon_smooth(bool enable);
void GPU_program_point_size(bool enable);
void GPU_scissor(int x, int y, int width, int height);
void GPU_scissor_get_f(float coords[4]);
void GPU_scissor_get_i(int coords[4]);
void GPU_viewport(int x, int y, int width, int height);
void GPU_viewport_size_get_f(float coords[4]);
void GPU_viewport_size_get_i(int coords[4]);
void GPU_color_mask(bool r, bool g, bool b, bool a);
void GPU_depth_mask(bool depth);
bool GPU_depth_mask_get(void);
void GPU_stencil_mask(uint stencil);
void GPU_unpack_row_length_set(uint len);
void GPU_clip_distances(int enabled_len);
bool GPU_mipmap_enabled(void);

void GPU_flush(void);
void GPU_finish(void);

void GPU_logic_op_xor_set(bool enable);

/* Attribute push & pop. */
typedef enum eGPUAttrMask {
  GPU_DEPTH_BUFFER_BIT = (1 << 0),
  GPU_ENABLE_BIT = (1 << 1),
  GPU_SCISSOR_BIT = (1 << 2),
  GPU_VIEWPORT_BIT = (1 << 3),
  GPU_BLEND_BIT = (1 << 4),
} eGPUAttrMask;

void gpuPushAttr(eGPUAttrMask mask);
void gpuPopAttr(void);

#ifdef __cplusplus
}
#endif
