/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/gpu_clip_planes_infos.hh"

#ifdef GPU_FRAGMENT_SHADER
#  error File should not be included in fragment shader
#endif

#ifdef USE_WORLD_CLIP_PLANES

VERTEX_SHADER_CREATE_INFO(gpu_clip_planes)

void world_clip_planes_calc_clip_distance(float3 wpos)
{
  float4 pos = float4(wpos, 1.0f);

  gl_ClipDistance[0] = dot(clipPlanes.world[0], pos);
  gl_ClipDistance[1] = dot(clipPlanes.world[1], pos);
  gl_ClipDistance[2] = dot(clipPlanes.world[2], pos);
  gl_ClipDistance[3] = dot(clipPlanes.world[3], pos);
  gl_ClipDistance[4] = dot(clipPlanes.world[4], pos);
  gl_ClipDistance[5] = dot(clipPlanes.world[5], pos);
}

#endif
