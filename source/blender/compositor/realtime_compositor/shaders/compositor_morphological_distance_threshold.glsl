/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The Morphological Distance Threshold operation is effectively three consecutive operations
 * implemented as a single operation. The three operations are as follows:
 *
 * .-----------.   .--------------.   .----------------.
 * | Threshold |-->| Dilate/Erode |-->| Distance Inset |
 * '-----------'   '--------------'   '----------------'
 *
 * The threshold operation just converts the input into a binary image, where the pixel is 1 if it
 * is larger than 0.5 and 0 otherwise. Pixels that are 1 in the output of the threshold operation
 * are said to be masked. The dilate/erode operation is a dilate or erode morphological operation
 * with a circular structuring element depending on the sign of the distance, where it is a dilate
 * operation if the distance is positive and an erode operation otherwise. This is equivalent to
 * the Morphological Distance operation, see its implementation for more information. Finally, the
 * distance inset is an operation that converts the binary image into a narrow band distance field.
 * That is, pixels that are unmasked will remain 0, while pixels that are masked will start from
 * zero at the boundary of the masked region and linearly increase until reaching 1 in the span of
 * a number pixels given by the inset value.
 *
 * As a performance optimization, the dilate/erode operation is omitted and its effective result is
 * achieved by slightly adjusting the distance inset operation. The base distance inset operation
 * works by computing the signed distance from the current center pixel to the nearest pixel with a
 * different value. Since our image is a binary image, that means that if the pixel is masked, we
 * compute the signed distance to the nearest unmasked pixel, and if the pixel unmasked, we compute
 * the signed distance to the nearest masked pixel. The distance is positive if the pixel is masked
 * and negative otherwise. The distance is then normalized by dividing by the given inset value and
 * clamped to the [0, 1] range. Since distances larger than the inset value are eventually clamped,
 * the distance search window is limited to a radius equivalent to the inset value.
 *
 * To archive the effective result of the omitted dilate/erode operation, we adjust the distance
 * inset operation as follows. First, we increase the radius of the distance search window by the
 * radius of the dilate/erode operation. Then we adjust the resulting narrow band signed distance
 * field as follows.
 *
 * For the erode case, we merely subtract the erode distance, which makes the outermost erode
 * distance number of pixels zero due to clamping, consequently achieving the result of the erode,
 * while retaining the needed inset because we increased the distance search window by the same
 * amount we subtracted.
 *
 * Similarly, for the dilate case, we add the dilate distance, which makes the dilate distance
 * number of pixels just outside of the masked region positive and part of the narrow band distance
 * field, consequently achieving the result of the dilate, while at the same time, the innermost
 * dilate distance number of pixels become 1 due to clamping, retaining the needed inset because we
 * increased the distance search window by the same amount we added.
 *
 * Since the erode/dilate distance is already signed appropriately as described before, we just add
 * it in both cases. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Apply a threshold operation on the center pixel, where the threshold is currently hard-coded
   * at 0.5. The pixels with values larger than the threshold are said to be masked. */
  bool is_center_masked = texture_load(input_tx, texel).x > 0.5;

  /* Since the distance search window will access pixels outside of the bounds of the image, we use
   * a texture loader with a fallback value. And since we don't want those values to affect the
   * result, the fallback value is chosen such that the inner condition fails, which is when the
   * sampled pixel and the center pixel are the same, so choose a fallback that will be considered
   * masked if the center pixel is masked and unmasked otherwise. */
  vec4 fallback = vec4(is_center_masked ? 1.0 : 0.0);

  /* Since the distance search window is limited to the given radius, the maximum possible squared
   * distance to the center is double the squared radius. */
  int minimum_squared_distance = radius * radius * 2;

  /* Find the squared distance to the nearest different pixel in the search window of the given
   * radius. */
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      bool is_sample_masked = texture_load(input_tx, texel + ivec2(x, y), fallback).x > 0.5;
      if (is_center_masked != is_sample_masked) {
        minimum_squared_distance = min(minimum_squared_distance, x * x + y * y);
      }
    }
  }

  /* Compute the actual distance from the squared distance and assign it an appropriate sign
   * depending on whether it lies in a masked region or not. */
  float signed_minimum_distance = sqrt(float(minimum_squared_distance)) *
                                  (is_center_masked ? 1.0 : -1.0);

  /* Add the erode/dilate distance and divide by the inset amount as described in the discussion,
   * then clamp to the [0, 1] range. */
  float value = clamp((signed_minimum_distance + distance) / inset, 0.0, 1.0);

  imageStore(output_img, texel, vec4(value));
}
