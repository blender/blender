/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_overlays_stereo_merge_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_stereo_merge)

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);
}
