/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Find the minimum/maximum value in the circular window of the given radius around the pixel. By
   * circular window, we mean that pixels in the window whose distance to the center of window is
   * larger than the given radius are skipped and not considered. Consequently, the dilation or
   * erosion that take place produces round results as opposed to squarish ones. This is
   * essentially a morphological operator with a circular structuring element. The LIMIT value
   * should be FLT_MAX if OPERATOR is min and FLT_MIN if OPERATOR is max. */
  float value = LIMIT;
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      if (x * x + y * y <= radius * radius) {
        value = OPERATOR(value, texture_load(input_tx, texel + ivec2(x, y), vec4(LIMIT)).x);
      }
    }
  }

  imageStore(output_img, texel, vec4(value));
}
