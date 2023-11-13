/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 center_determinator = texture_load(determinator_tx, texel);

  /* Go over the pixels in the blur window of the specified radius around the center pixel, and for
   * pixels whose determinator is close enough to the determinator of the center pixel, accumulate
   * their color as well as their weights. */
  float accumulated_weight = 0.0;
  vec4 accumulated_color = vec4(0.0);
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      vec4 determinator = texture_load(determinator_tx, texel + ivec2(x, y));
      float difference = dot(abs(center_determinator - determinator).rgb, vec3(1.0));

      if (difference < threshold) {
        accumulated_weight += 1.0;
        accumulated_color += texture_load(input_tx, texel + ivec2(x, y));
      }
    }
  }

  /* Write the accumulated color divided by the accumulated weight if any pixel in the window was
   * accumulated, otherwise, write a fallback black color. */
  vec4 fallback = vec4(vec3(0.0), 1.0);
  vec4 color = (accumulated_weight != 0.0) ? (accumulated_color / accumulated_weight) : fallback;
  imageStore(output_img, texel, color);
}
