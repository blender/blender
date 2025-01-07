/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));
  vec4 glare_value = texture(input_tx, normalized_coordinates);
  vec4 adjusted_glare_value = glare_value * strength;
  imageStore(output_img, texel, vec4(adjusted_glare_value.rgb, 1.0));
}
