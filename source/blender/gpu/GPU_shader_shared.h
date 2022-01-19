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
 *
 * The Original Code is Copyright (C) 2022 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  include "intern/gpu_shader_shared_utils.h"
#endif

#ifdef __cplusplus
using blender::float2;
using blender::float3;
using blender::float4;
using blender::float4x4;
#endif

struct NodeLinkData {
  float4 colors[3];
  /* bezierPts Is actually a float2, but due to std140 each element needs to be aligned to 16
   * bytes. */
  float4 bezierPts[4];
  bool1 doArrow;
  bool1 doMuted;
  float dim_factor;
  float thickness;
  float dash_factor;
  float dash_alpha;
  float expandSize;
  float arrowSize;
};
BLI_STATIC_ASSERT_ALIGN(struct NodeLinkData, 16)

struct NodeLinkInstanceData {
  float4 colors[6];
  float expandSize;
  float arrowSize;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct NodeLinkInstanceData, 16)

struct GPencilStrokeData {
  float2 viewport;
  float pixsize;
  float objscale;
  float pixfactor;
  int xraymode;
  int caps_start;
  int caps_end;
  bool1 keep_size;
  bool1 fill_stroke;
  float2 _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct GPencilStrokeData, 16)

struct GPUClipPlanes {
  float4x4 ModelMatrix;
  float4 world[6];
};
BLI_STATIC_ASSERT_ALIGN(struct GPUClipPlanes, 16)

struct SimpleLightingData {
  float4 color;
  float3 light;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(struct SimpleLightingData, 16)

#define MAX_CALLS 16

struct MultiRectCallData {
  float4 calls_data[MAX_CALLS * 3];
};
BLI_STATIC_ASSERT_ALIGN(struct MultiRectCallData, 16)
