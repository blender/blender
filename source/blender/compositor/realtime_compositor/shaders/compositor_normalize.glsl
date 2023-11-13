/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  float value = texture_load(input_tx, texel).x;
  float normalized_value = (value - minimum) * scale;
  float clamped_value = clamp(normalized_value, 0.0, 1.0);
  imageStore(output_img, texel, vec4(clamped_value));
}
