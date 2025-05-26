/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const int2 size = imageSize(output_img);
  const float2 centered_coordinates = (float2(texel) + 0.5f) - float2(size) / 2.0f;

  const int max_size = max(size.x, size.y);
  const float2 normalized_coordinates = (centered_coordinates / max_size) * 2.0f;

  imageStore(output_img, texel, float4(normalized_coordinates, float2(0.0f)));
}
