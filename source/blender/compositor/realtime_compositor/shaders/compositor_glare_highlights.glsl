/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void main()
{
  /* The dispatch domain covers the output image size, which might be a fraction of the input image
   * size, so you will notice the output image size used throughout the shader instead of the input
   * one. */
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));

  vec4 hsva;
  rgb_to_hsv(texture(input_tx, normalized_coordinates), hsva);

  /* The pixel whose luminance value is less than the threshold luminance is not considered part of
   * the highlights and is given a value of zero. Otherwise, the pixel is considered part of the
   * highlights, whose luminance value is the difference to the threshold. */
  hsva.z = max(0.0, hsva.z - threshold);

  vec4 rgba;
  hsv_to_rgb(hsva, rgba);

  imageStore(output_img, texel, vec4(rgba.rgb, 1.0));
}
