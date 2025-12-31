/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

void view_clipping_distances(float3 wpos)
{
#if defined(GPU_VERTEX_SHADER)
  VERTEX_SHADER_CREATE_INFO(drw_clipped)
#  ifdef USE_WORLD_CLIP_PLANES
  float4 pos_4d = float4(wpos, 1.0f);
  gl_ClipDistance[0] = dot(drw_clipping_[0], pos_4d);
  gl_ClipDistance[1] = dot(drw_clipping_[1], pos_4d);
  gl_ClipDistance[2] = dot(drw_clipping_[2], pos_4d);
  gl_ClipDistance[3] = dot(drw_clipping_[3], pos_4d);
  gl_ClipDistance[4] = dot(drw_clipping_[4], pos_4d);
  gl_ClipDistance[5] = dot(drw_clipping_[5], pos_4d);
#  endif
#endif
}

void view_clipping_distances_bypass()
{
#if defined(GPU_VERTEX_SHADER)
  VERTEX_SHADER_CREATE_INFO(drw_clipped)
#  ifdef USE_WORLD_CLIP_PLANES
  gl_ClipDistance[0] = 1.0f;
  gl_ClipDistance[1] = 1.0f;
  gl_ClipDistance[2] = 1.0f;
  gl_ClipDistance[3] = 1.0f;
  gl_ClipDistance[4] = 1.0f;
  gl_ClipDistance[5] = 1.0f;
#  endif
#endif
}
