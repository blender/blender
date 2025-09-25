/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_point_varying_size_varying_color_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_point_varying_size_varying_color)

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);
  gl_PointSize = size;
  finalColor = color;
}
