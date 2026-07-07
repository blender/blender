/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Vertex Shader for dashed lines with 3D coordinates,
 * with uniform multi-colors or uniform single-color, and unary thickness.
 *
 * Dashed is performed in screen space.
 */

#include "infos/gpu_shader_line_dashed_uniform_color_infos.hh"

#include "gpu_shader_cfg_world_clip_lib.glsl"

VERTEX_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color_clipped)

void main()
{
  float4 pos_4d = float4(pos, 1.0f);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  stipple_start = stipple_pos = viewport_size * 0.5f * (gl_Position.xy / gl_Position.w);
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
