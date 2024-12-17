/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lookdev_info.hh"

VERTEX_SHADER_CREATE_INFO(eevee_lookdev_display)

void main()
{
  uint vert_index = gl_VertexID < 3 ? gl_VertexID : gl_VertexID - 2;

  vec2 uv = vec2(vert_index / 2, vert_index % 2);
  uv_coord = uv;
  sphere_id = gpu_InstanceIndex;

  vec2 sphere_size = vec2(textureSize(metallic_tx, 0)) * invertedViewportSize;
  vec2 margin = vec2(0.125, -0.125) * sphere_size;
  vec2 anchor_point = vec2(1.0, -1.0) -
                      vec2(viewportSize.x - anchor.x, anchor.y) * invertedViewportSize *
                          vec2(2.0) -
                      margin;

  vec2 offset = anchor_point -
                vec2(sphere_size.x * (gpu_InstanceIndex + 1) + margin.x * 2.0 * gpu_InstanceIndex,
                     0.0);
  vec2 co = uv * sphere_size + offset;
  gl_Position = vec4(co, 0.0, 1.0);
}
