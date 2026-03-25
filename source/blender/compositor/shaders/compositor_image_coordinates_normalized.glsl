/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_image_coordinates_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_image_coordinates_normalized)

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const float2 coordinates = float2(data_offset + texel) + 0.5f;
  const float2 normalized_coordinates = coordinates / float2(display_size);

  imageStore(output_img, texel, float4(normalized_coordinates, float2(0.0f)));
}
