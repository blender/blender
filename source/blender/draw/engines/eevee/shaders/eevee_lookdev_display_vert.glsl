/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lookdev_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_lookdev_display)

void main()
{
  uint vert_index = gl_VertexID < 3 ? gl_VertexID : gl_VertexID - 2;

  float2 uv = float2(vert_index / 2, vert_index % 2);
  uv_coord = uv;
  sphere_id = gpu_InstanceIndex;

  float2 sphere_size = float2(textureSize(metallic_tx, 0)) * invertedViewportSize;
  float2 margin = float2(0.125f, -0.125f) * sphere_size;
  float2 anchor_point = float2(1.0f, -1.0f) -
                        float2(viewportSize.x - anchor.x, anchor.y) * invertedViewportSize *
                            float2(2.0f) -
                        margin;

  float2 offset = anchor_point - float2(sphere_size.x * (gpu_InstanceIndex + 1) +
                                            margin.x * 2.0f * gpu_InstanceIndex,
                                        0.0f);
  float2 co = uv * sphere_size + offset;
  gl_Position = float4(co, 0.0f, 1.0f);
}
