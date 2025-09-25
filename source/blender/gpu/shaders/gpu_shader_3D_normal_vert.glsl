/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_simple_lighting_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_simple_lighting)

void main()
{
  normal = normalize(NormalMatrix * nor);
  gl_Position = ModelViewProjectionMatrix * float4(pos, 1.0f);
}
