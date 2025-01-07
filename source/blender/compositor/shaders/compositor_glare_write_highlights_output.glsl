/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));
  imageStore(output_img, texel, texture(input_tx, normalized_coordinates));
}
