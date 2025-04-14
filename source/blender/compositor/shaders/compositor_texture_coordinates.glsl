/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  int2 size = imageSize(output_img);
  float2 centered_coordinates = (float2(texel) + 0.5f) - float2(size) / 2.0f;

  int max_size = max(size.x, size.y);
  float2 normalized_coordinates = (centered_coordinates / max_size) * 2.0f;

  imageStore(output_img, texel, float4(normalized_coordinates, float2(0.0f)));
}
