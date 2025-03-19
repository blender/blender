/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  ivec2 size = imageSize(output_img);
  vec2 centered_coordinates = (vec2(texel) + 0.5) - vec2(size) / 2.0;

  int max_size = max(size.x, size.y);
  vec2 normalized_coordinates = (centered_coordinates / max_size) * 2.0;

  imageStore(output_img, texel, vec4(normalized_coordinates, vec2(0.0)));
}
