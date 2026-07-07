/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_point_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_3D_point_uniform_size_uniform_color_aa)

void main()
{
  float4 pos_4d = float4(pos, 1.0f);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  gl_PointSize = size;

  /* Calculate concentric radii in pixels. */
  float radius = 0.5f * size;

  /* Start at the outside and progress toward the center. */
  radii[0] = radius;
  radii[1] = radius - 1.0f;

  /* Convert to PointCoord units. */
  radii /= size;
}
