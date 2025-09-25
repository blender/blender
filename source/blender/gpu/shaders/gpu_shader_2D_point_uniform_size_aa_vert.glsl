/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_point_uniform_size_uniform_color_aa_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_aa)

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);
  gl_PointSize = size;

  /* calculate concentric radii in pixels */
  float radius = 0.5f * size;

  /* start at the outside and progress toward the center */
  radii[0] = radius;
  radii[1] = radius - 1.0f;

  /* convert to PointCoord units */
  radii /= size;
}
