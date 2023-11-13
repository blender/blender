/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(preview_img));
  imageStore(preview_img, texel, texture(input_tx, coordinates));
}
