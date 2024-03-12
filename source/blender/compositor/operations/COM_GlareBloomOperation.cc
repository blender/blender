/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <cstring>
#include <memory>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "COM_GlareBloomOperation.h"

#define MAX_GLARE_SIZE 9

namespace blender::compositor {

void upsample(const MemoryBuffer &input, MemoryBuffer &output)
{
  const int2 output_size = int2(output.get_width(), output.get_height());

  /* All the offsets in the following code section are in the normalized pixel space of the output
   * image, so compute its normalized pixel size. */
  float2 pixel_size = 1.0f / float2(output_size);

  threading::parallel_for(IndexRange(output_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(output_size.x)) {
        /* Each invocation corresponds to one output pixel, where the output has twice the size of
         * the input. */
        int2 texel = int2(x, y);

        /* Add 0.5 to evaluate the buffer at the center of the pixel and divide by the image size
         * to get the coordinates into the buffer's expected [0, 1] range. */
        float2 coordinates = (float2(texel) + float2(0.5)) / float2(output_size);

        /* Upsample by applying a 3x3 tent filter on the bi-linearly interpolated values evaluated
         * at the center of neighboring output pixels. As more tent filter upsampling passes are
         * applied, the result approximates a large sized Gaussian filter. This upsampling strategy
         * is described in the talk:
         *
         *   Next Generation Post Processing in Call of Duty: Advanced Warfare
         *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
         *
         * In particular, the upsampling strategy is described and illustrated in slide 162 titled
         * "Upsampling - Our Solution". */
        float4 upsampled = float4(0.0f);
        upsampled += (4.0f / 16.0f) * input.texture_bilinear_extend(coordinates);
        upsampled += (2.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(-1.0f, 0.0f));
        upsampled += (2.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(0.0f, 1.0f));
        upsampled += (2.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(1.0f, 0.0f));
        upsampled += (2.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(0.0f, -1.0f));
        upsampled += (1.0f / 16.0f) * input.texture_bilinear_extend(
                                          coordinates + pixel_size * float2(-1.0f, -1.0f));
        upsampled += (1.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(-1.0f, 1.0f));
        upsampled += (1.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(1.0f, -1.0f));
        upsampled += (1.0f / 16.0f) *
                     input.texture_bilinear_extend(coordinates + pixel_size * float2(1.0f, 1.0f));

        const float4 original_value = output.get_elem(texel.x, texel.y);
        copy_v4_v4(output.get_elem(texel.x, texel.y), original_value + upsampled);
      }
    }
  });
}

/* Computes the weighted average of the given four colors, which are assumed to the colors of
 * spatially neighboring pixels. The weights are computed so as to reduce the contributions of
 * fireflies on the result by applying a form of local tone mapping as described by Brian Karis in
 * the article "Graphic Rants: Tone Mapping".
 *
 * https://graphicrants.blogspot.com/2013/12/tone-mapping.html */
static float4 karis_brightness_weighted_sum(float4 color1,
                                            float4 color2,
                                            float4 color3,
                                            float4 color4)
{
  const float4 brightness = float4(math::reduce_max(color1.xyz()),
                                   math::reduce_max(color2.xyz()),
                                   math::reduce_max(color3.xyz()),
                                   math::reduce_max(color4.xyz()));
  const float4 weights = 1.0f / (brightness + 1.0);
  const float weights_sum = math::reduce_add(weights);
  const float4 sum = color1 * weights[0] + color2 * weights[1] + color3 * weights[2] +
                     color4 * weights[3];
  return math::safe_divide(sum, weights_sum);
}

static void downsample(const MemoryBuffer &input, MemoryBuffer &output, bool use_karis_average)
{
  const int2 input_size = int2(input.get_width(), input.get_height());
  const int2 output_size = int2(output.get_width(), output.get_height());

  /* All the offsets in the following code section are in the normalized pixel space of the
   * input.texture_bilinear_extend, so compute its normalized pixel size. */
  float2 pixel_size = 1.0f / float2(input_size);

  threading::parallel_for(IndexRange(output_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(output_size.x)) {
        /* Each invocation corresponds to one output pixel, where the output has half the size of
         * the input. */
        int2 texel = int2(x, y);

        /* Add 0.5 to evaluate the buffer at the center of the pixel and divide by the image size
         * to get the coordinates into the buffer's expected [0, 1] range. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(output_size);

        /* Each invocation downsamples a 6x6 area of pixels around the center of the corresponding
         * output pixel, but instead of sampling each of the 36 pixels in the area, we only sample
         * 13 positions using bilinear fetches at the center of a number of overlapping square
         * 4-pixel groups. This downsampling strategy is described in the talk:
         *
         *   Next Generation Post Processing in Call of Duty: Advanced Warfare
         *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
         *
         * In particular, the downsampling strategy is described and illustrated in slide 153
         * titled "Downsampling - Our Solution". This is employed as it significantly improves the
         * stability of the glare as can be seen in the videos in the talk. */
        float4 center = input.texture_bilinear_extend(coordinates);
        float4 upper_left_near = input.texture_bilinear_extend(coordinates +
                                                               pixel_size * float2(-1.0f, 1.0f));
        float4 upper_right_near = input.texture_bilinear_extend(coordinates +
                                                                pixel_size * float2(1.0f, 1.0f));
        float4 lower_left_near = input.texture_bilinear_extend(coordinates +
                                                               pixel_size * float2(-1.0f, -1.0f));
        float4 lower_right_near = input.texture_bilinear_extend(coordinates +
                                                                pixel_size * float2(1.0f, -1.0f));
        float4 left_far = input.texture_bilinear_extend(coordinates +
                                                        pixel_size * float2(-2.0f, 0.0f));
        float4 right_far = input.texture_bilinear_extend(coordinates +
                                                         pixel_size * float2(2.0f, 0.0f));
        float4 upper_far = input.texture_bilinear_extend(coordinates +
                                                         pixel_size * float2(0.0f, 2.0f));
        float4 lower_far = input.texture_bilinear_extend(coordinates +
                                                         pixel_size * float2(0.0f, -2.0f));
        float4 upper_left_far = input.texture_bilinear_extend(coordinates +
                                                              pixel_size * float2(-2.0f, 2.0f));
        float4 upper_right_far = input.texture_bilinear_extend(coordinates +
                                                               pixel_size * float2(2.0f, 2.0f));
        float4 lower_left_far = input.texture_bilinear_extend(coordinates +
                                                              pixel_size * float2(-2.0f, -2.0f));
        float4 lower_right_far = input.texture_bilinear_extend(coordinates +
                                                               pixel_size * float2(2.0f, -2.0f));

        if (!use_karis_average) {
          /* The original weights equation mentioned in slide 153 is:
           *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
           * The 0.5 corresponds to the center group of pixels and the 0.125 corresponds to the
           * other groups of pixels. The center is sampled 4 times, the far non corner pixels are
           * sampled 2 times, the near corner pixels are sampled only once; but their weight is
           * quadruple the weights of other groups; so they count as sampled 4 times, finally the
           * far corner pixels are sampled only once, essentially totaling 32 samples. So the
           * weights are as used in the following code section. */
          float4 result = (4.0f / 32.0f) * center +
                          (4.0f / 32.0f) * (upper_left_near + upper_right_near + lower_left_near +
                                            lower_right_near) +
                          (2.0f / 32.0f) * (left_far + right_far + upper_far + lower_far) +
                          (1.0f / 32.0f) * (upper_left_far + upper_right_far + lower_left_far +
                                            lower_right_far);
          copy_v4_v4(output.get_elem(texel.x, texel.y), result);
        }
        else {
          /* Reduce the contributions of fireflies on the result by reducing each group of pixels
           * using a Karis brightness weighted sum. This is described in slide 168 titled
           * "Fireflies - Partial Karis Average".
           *
           * This needn't be done on all downsampling passes, but only the first one, since
           * fireflies will not survive the first pass, later passes can use the weighted average.
           */
          float4 center_weighted_sum = karis_brightness_weighted_sum(
              upper_left_near, upper_right_near, lower_right_near, lower_left_near);
          float4 upper_left_weighted_sum = karis_brightness_weighted_sum(
              upper_left_far, upper_far, center, left_far);
          float4 upper_right_weighted_sum = karis_brightness_weighted_sum(
              upper_far, upper_right_far, right_far, center);
          float4 lower_right_weighted_sum = karis_brightness_weighted_sum(
              center, right_far, lower_right_far, lower_far);
          float4 lower_left_weighted_sum = karis_brightness_weighted_sum(
              left_far, center, lower_far, lower_left_far);

          /* The original weights equation mentioned in slide 153 is:
           *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
           * Multiply both sides by 8 and you get:
           *   4 + 1 + 1 + 1 + 1 = 8
           * So the weights are as used in the following code section. */
          float4 result = (4.0f / 8.0f) * center_weighted_sum +
                          (1.0f / 8.0f) * (upper_left_weighted_sum + upper_right_weighted_sum +
                                           lower_left_weighted_sum + lower_right_weighted_sum);
          copy_v4_v4(output.get_elem(texel.x, texel.y), result);
        }
      }
    }
  });
}

/* Progressively down-sample the given buffer into a buffer with half the size for the given
 * chain length, returning an array containing the chain of down-sampled buffers. The first
 * buffer of the chain is the given buffer itself for easier handling. The chain length is
 * expected not to exceed the binary logarithm of the smaller dimension of the given buffer,
 * because that would buffer in down-sampling passes that produce useless textures with just
 * one pixel. */
static Array<std::unique_ptr<MemoryBuffer>> compute_bloom_downsample_chain(
    MemoryBuffer &highlights, int chain_length)
{
  Array<std::unique_ptr<MemoryBuffer>> downsample_chain(chain_length);

  /* We append the original highlights buffer to the first buffer of the chain to make the code
   * easier. In turn, the number of passes is one less than the chain length, because the first
   * buffer needn't be computed. */
  downsample_chain[0] = std::make_unique<MemoryBuffer>(highlights);
  const IndexRange downsample_passes_range(chain_length - 1);

  for (const int i : downsample_passes_range) {
    const MemoryBuffer &input = *downsample_chain[i];

    const int2 input_size = int2(input.get_width(), input.get_height());
    const int2 output_size = input_size / 2;

    rcti output_rect;
    BLI_rcti_init(&output_rect, 0, output_size.x, 0, output_size.y);
    downsample_chain[i + 1] = std::make_unique<MemoryBuffer>(DataType::Color, output_rect, false);
    MemoryBuffer &output = *downsample_chain[i + 1];

    /* For the first down-sample pass, we use a special "Karis" down-sample pass that applies a
     * form of local tone mapping to reduce the contributions of fireflies, see the shader for
     * more information. Later passes use a simple average down-sampling filter because fireflies
     * doesn't service the first pass. */
    const bool use_karis_average = i == downsample_passes_range.first();
    downsample(input, output, use_karis_average);
  }

  return downsample_chain;
}

/* The size of the bloom relative to its maximum possible size, see the
 * compute_bloom_size_halving_count() method for more information. */
static int get_bloom_size(const NodeGlare *settings)
{
  return settings->size;
}

/* The bloom has a maximum possible size when the bloom size is equal to MAX_GLARE_SIZE and
 * halves for every unit decrement of the bloom size. This method computes the number of halving
 * that should take place, which is simply the difference to MAX_GLARE_SIZE. */
static int compute_bloom_size_halving_count(const NodeGlare *settings)
{
  return MAX_GLARE_SIZE - get_bloom_size(settings);
}

/* Bloom is computed by first progressively half-down-sampling the highlights down to a certain
 * size, then progressively double-up-sampling the last down-sampled buffer up to the original size
 * of the highlights, adding the down-sampled buffer of the same size in each up-sampling step.
 * This can be illustrated as follows:
 *
 *             Highlights   ---+--->  Bloom
 *                  |                   |
 *             Down-sampled ---+---> Up-sampled
 *                  |                   |
 *             Down-sampled ---+---> Up-sampled
 *                  |                   |
 *             Down-sampled ---+---> Up-sampled
 *                  |                   ^
 *                 ...                  |
 *            Down-sampled  ------------'
 *
 * The smooth down-sampling followed by smooth up-sampling can be thought of as a cheap way to
 * approximate a large radius blur, and adding the corresponding down-sampled buffer while
 * up-sampling is done to counter the attenuation that happens during down-sampling.
 *
 * Smaller down-sampled buffers contribute to larger glare size, so controlling the size can be
 * done by stopping down-sampling down to a certain size, where the maximum possible size is
 * achieved when down-sampling happens down to the smallest size of 2. */
void GlareBloomOperation::generate_glare(float *output,
                                         MemoryBuffer *highlights,
                                         const NodeGlare *settings)
{
  /* The maximum possible glare size is achieved when we down-sampled down to the smallest size
   * of 2, which would buffer in a down-sampling chain length of the binary logarithm of the
   * smaller dimension of the size of the highlights.
   *
   * However, as users might want a smaller glare size, we reduce the chain length by the halving
   * count supplied by the user. */
  const int2 size = int2(highlights->get_width(), highlights->get_height());
  const int smaller_glare_dimension = math::min(size.x, size.y);
  const int chain_length = int(std::log2(smaller_glare_dimension)) -
                           compute_bloom_size_halving_count(settings);

  Array<std::unique_ptr<MemoryBuffer>> downsample_chain = compute_bloom_downsample_chain(
      *highlights, chain_length);

  /* Notice that for a chain length of n, we need (n - 1) up-sampling passes. */
  const IndexRange upsample_passes_range(chain_length - 1);

  for (const int i : upsample_passes_range) {
    const MemoryBuffer &input = *downsample_chain[upsample_passes_range.last() - i + 1];
    MemoryBuffer &output = *downsample_chain[upsample_passes_range.last() - i];
    upsample(input, output);
  }

  memcpy(output,
         downsample_chain[0]->get_buffer(),
         size.x * size.y * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float));
}

}  // namespace blender::compositor
