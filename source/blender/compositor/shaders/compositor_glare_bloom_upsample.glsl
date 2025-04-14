/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* Each invocation corresponds to one output pixel, where the output has twice the size of the
   * input. */
  int2 texel = int2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to
   * get the coordinates into the sampler's expected [0, 1] range. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(imageSize(output_img));

  /* All the offsets in the following code section are in the normalized pixel space of the output
   * image, so compute its normalized pixel size. */
  float2 pixel_size = 1.0f / float2(imageSize(output_img));

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
  float4 upsampled = float4(0.0f);
  upsampled += (4.0f / 16.0f) * texture(input_tx, coordinates);
  upsampled += (2.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(-1.0f, 0.0f));
  upsampled += (2.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(0.0f, 1.0f));
  upsampled += (2.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(1.0f, 0.0f));
  upsampled += (2.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(0.0f, -1.0f));
  upsampled += (1.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(-1.0f, -1.0f));
  upsampled += (1.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(-1.0f, 1.0f));
  upsampled += (1.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(1.0f, -1.0f));
  upsampled += (1.0f / 16.0f) * texture(input_tx, coordinates + pixel_size * float2(1.0f, 1.0f));

  float4 combined = imageLoad(output_img, texel) + upsampled;
  imageStore(output_img, texel, float4(combined.rgb, 1.0f));
}
