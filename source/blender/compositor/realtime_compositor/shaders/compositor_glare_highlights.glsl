/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* The dispatch domain covers the output image size, which might be a fraction of the input image
   * size, so you will notice the output image size used throughout the shader instead of the input
   * one. */
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  vec2 normalized_coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));

  vec4 input_color = texture(input_tx, normalized_coordinates);
  float luminance = dot(input_color.rgb, luminance_coefficients);

  /* The pixel whose luminance is less than the threshold luminance is not considered part of the
   * highlights and is given a value of zero. Otherwise, the pixel is considered part of the
   * highlights, whose value is the difference to the threshold value clamped to zero. */
  bool is_highlights = luminance >= threshold;
  vec3 highlights = is_highlights ? max(vec3(0.0), input_color.rgb - threshold) : vec3(0.0);

  imageStore(output_img, texel, vec4(highlights, 1.0));
}
