/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_glare_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_glare_kernel_downsample_color)

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  const float2 normalized_coordinates = (float2(texel) + float2(0.5f)) /
                                        float2(imageSize(output_img));
  imageStore(output_img, texel, texture(input_tx, normalized_coordinates));
}
