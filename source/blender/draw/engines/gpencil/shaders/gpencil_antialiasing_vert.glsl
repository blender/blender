/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpencil_antialiasing_stage_1)

#include "gpu_shader_smaa_lib.glsl"

void main()
{
  int v = gl_VertexID % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  gl_Position = float4(x, y, 1.0f, 1.0f);
  uvs = (gl_Position.xy + 1.0f) * 0.5f;

  float4 offset[3];

#if SMAA_STAGE == 0
  SMAAEdgeDetectionVS(uvs, offset);
#elif SMAA_STAGE == 1
  SMAABlendingWeightCalculationVS(uvs, pixcoord, offset);
#elif SMAA_STAGE == 2
  SMAANeighborhoodBlendingVS(uvs, offset[0]);
#endif

  offset0 = offset[0];
  offset1 = offset[1];
  offset2 = offset[2];
}
