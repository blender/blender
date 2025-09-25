/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_uniform_color_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_3D_clipped_uniform_color)

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos, 1.0f);
  gl_ClipDistance[0] = dot(ModelMatrix * float4(pos, 1.0f), ClipPlane);
}
