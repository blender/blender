/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  include "GPU_shader_shared_utils.h"
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
