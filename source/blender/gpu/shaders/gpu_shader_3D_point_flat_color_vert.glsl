/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_point_infos.hh"

#include "gpu_shader_cfg_world_clip_lib.glsl"

VERTEX_SHADER_CREATE_INFO(gpu_shader_3D_point_flat_color)

void main()
{
  float4 pos_4d = float4(pos, 1.0f);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  gl_PointSize = size;
  finalColor = color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((clipPlanes.ClipModelMatrix * pos_4d).xyz);
#endif
}
