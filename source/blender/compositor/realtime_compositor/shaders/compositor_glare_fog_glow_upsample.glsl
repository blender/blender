/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* Each invocation corresponds to one output pixel, where the output has twice the size of the
   * input. */
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to get
   * the coordinates into the sampler's expected [0, 1] range. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(output_img));

  /* All the offsets in the following code section are in the normalized pixel space of the output
   * image, so compute its normalized pixel size. */
  vec2 pixel_size = 1.0 / vec2(imageSize(output_img));

  /* Upsample by applying a 3x3 tent filter on the bi-linearly interpolated values evaluated at
   * the center of neighboring output pixels. As more tent filter upsampling passes are applied,
   * the result approximates a large sized Gaussian filter. This upsampling strategy is described
   * in the talk:
   *
   *   Next Generation Post Processing in Call of Duty: Advanced Warfare
   *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
   *
   * In particular, the upsampling strategy is described and illustrated in slide 162 titled
   * "Upsampling - Our Solution". */
  vec4 upsampled = vec4(0.0);
  upsampled += (4.0 / 16.0) * texture(input_tx, coordinates);
  upsampled += (2.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(-1.0, 0.0));
  upsampled += (2.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(0.0, 1.0));
  upsampled += (2.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(1.0, 0.0));
  upsampled += (2.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(0.0, -1.0));
  upsampled += (1.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(-1.0, -1.0));
  upsampled += (1.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(-1.0, 1.0));
  upsampled += (1.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(1.0, -1.0));
  upsampled += (1.0 / 16.0) * texture(input_tx, coordinates + pixel_size * vec2(1.0, 1.0));

  imageStore(output_img, texel, imageLoad(output_img, texel) + upsampled);
}
