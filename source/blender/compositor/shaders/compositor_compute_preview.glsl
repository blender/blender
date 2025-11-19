/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_compute_preview_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_compute_preview)

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(imageSize(preview_img));
  imageStore(preview_img, texel, texture(input_tx, coordinates));
}
