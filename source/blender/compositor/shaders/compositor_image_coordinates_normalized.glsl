/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const int2 size = imageSize(output_img);
  const float2 normalized_coordinates = (float2(texel) + 0.5f) / float2(size);

  imageStore(output_img, texel, float4(normalized_coordinates, float2(0.0f)));
}
