/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_image_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_3D_image_common)

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos.xyz, 1.0f);
  texCoord_interp = texCoord;
}
