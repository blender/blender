/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_context.hh"
#include "COM_morphological_distance_feather_weights.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* The Morphological Distance Feather operation is a linear combination between the result of two
 * operations. The first operation is a Gaussian blur with a radius equivalent to the dilate/erode
 * distance, which is straightforward and implemented as a separable filter similar to the blur
 * operation.
 *
 * The second operation is an approximation of a morphological inverse distance operation evaluated
 * at a distance falloff function. The result of a morphological inverse distance operation is a
 * narrow band distance field that starts at its maximum value at boundaries where a difference in
 * values took place and linearly deceases until it reaches zero in the span of a number of pixels
 * equivalent to the erode/dilate distance. Additionally, instead of linearly decreasing, the user
 * may choose a different falloff which is evaluated at the computed distance. For dilation, the
 * distance field decreases outwards, and for erosion, the distance field decreased inwards.
 *
 * The reason why the result of a Gaussian blur is mixed in with the distance field is because the
 * distance field is merely approximated and not accurately computed, the defects of which is more
 * apparent away from boundaries and especially at corners where the distance field should take a
 * circular shape. That's why the Gaussian blur is mostly mixed only further from boundaries.
 *
 * The morphological inverse distance operation is approximated using a separable implementation
 * and intertwined with the Gaussian blur implementation as follows. A search window of a radius
 * equivalent to the dilate/erode distance is applied on the image to find either the minimum or
 * maximum pixel value multiplied by its corresponding falloff value in the window. For dilation,
 * we try to find the maximum, and for erosion, we try to find the minimum. The implementation uses
 * an inverse function to find the minimum. Additionally, we also save the falloff value where the
 * minimum or maximum was found. The found value will be that of the narrow band distance field and
 * the saved falloff value will be used as the mixing factor with the Gaussian blur.
 *
 * To make sense of the aforementioned algorithm, assume we are dilating a binary image by 5 pixels
 * whose half has a value of 1 and the other half has a value of zero. Consider the following:
 *
 * - A pixel of value 1 already has the maximum possible value, so its value will remain unchanged
 *   regardless of its position.
 * - A pixel of value 0 that is right at the boundary of the 1's region will have a maximum value
 *   of around 0.8 depending on the falloff. That's because the search window intersects the 1's
 *   region, which when multiplied by the falloff gives the first value of the falloff, which is
 *   larger than the initially zero value computed at the center of the search window.
 * - A pixel of value 0 that is 3 pixels away from the boundary will have a maximum value of around
 *   0.4 depending on the falloff. That's because the search window intersects the 1's region,
 *   which when multiplied by the falloff gives the third value of the falloff, which is larger
 *   than the initially zero value computed at the center of the search window.
 * - Finally, a pixel of value 0 that is 6 pixels away from the boundary will have a maximum value
 *   of 0, because the search window doesn't intersects the 1's region and only spans zero values.
 *
 * The previous example demonstrates how the distance field naturally arises, and the same goes for
 * the erode case, except the minimum value is computed instead. */
template<bool IsErode>
static void morphological_distance_feather_pass(const Result &input,
                                                const MorphologicalDistanceFeatherWeights &weights,
                                                Result &output)
{
  /* Notice that the size is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  const int2 size = int2(output.domain().size.y, output.domain().size.x);
  parallel_for(size, [&](const int2 texel) {
    /* A value for accumulating the blur result. */
    float accumulated_value = 0.0f;

    /* Compute the contribution of the center pixel to the blur result. */
    float center_value = input.load_pixel(texel).x;
    if constexpr (IsErode) {
      center_value = 1.0f - center_value;
    }
    accumulated_value += center_value * weights.weights_result.load_pixel(int2(0)).x;

    /* Start with the center value as the maximum/minimum distance and reassign to the true maximum
     * or minimum in the search loop below. Additionally, the center falloff is always 1.0, so
     * start with that. */
    float limit_distance = center_value;
    float limit_distance_falloff = 1.0f;

    /* Compute the contributions of the pixels to the right and left, noting that the weights and
     * falloffs textures only store the weights and falloffs for the positive half, but since the
     * they are both symmetric, the same weights and falloffs are used for the negative half and we
     * compute both of their contributions. */
    for (int i = 1; i < weights.weights_result.domain().size.x; i++) {
      float weight = weights.weights_result.load_pixel(int2(i, 0)).x;
      float falloff = weights.falloffs_result.load_pixel(int2(i, 0)).x;

      /* Loop for two iterations, where s takes the value of -1 and 1, which is used as the sign
       * needed to evaluated the positive and negative sides as explain above. */
      for (int s = -1; s < 2; s += 2) {
        /* Compute the contribution of the pixel to the blur result. */
        float value = input.load_pixel_extended(texel + int2(s * i, 0)).x;
        if constexpr (IsErode) {
          value = 1.0f - value;
        }
        accumulated_value += value * weight;

        /* The distance is computed such that its highest value is the pixel value itself, so
         * multiply the distance falloff by the pixel value. */
        float falloff_distance = value * falloff;

        /* Find either the maximum or the minimum for the dilate and erode cases respectively. */
        if (falloff_distance > limit_distance) {
          limit_distance = falloff_distance;
          limit_distance_falloff = falloff;
        }
      }
    }

    /* Mix between the limit distance and the blurred accumulated value such that the limit
     * distance is used for pixels closer to the boundary and the blurred value is used for pixels
     * away from the boundary. */
    float value = math::interpolate(accumulated_value, limit_distance, limit_distance_falloff);
    if constexpr (IsErode) {
      value = 1.0f - value;
    }

    /* Write the value using the transposed texel. See the horizontal pass function for more
     * information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), float4(value));
  });
}

static const char *get_shader_name(const int distance)
{
  if (distance > 0) {
    return "compositor_morphological_distance_feather_dilate";
  }
  return "compositor_morphological_distance_feather_erode";
}

static Result horizontal_pass_gpu(Context &context,
                                  const Result &input,
                                  const int distance,
                                  const int falloff_type)
{
  GPUShader *shader = context.get_shader(get_shader_name(distance));
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));
  weights.weights_result.bind_as_texture(shader, "weights_tx");
  weights.falloffs_result.bind_as_texture(shader, "falloffs_tx");

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will process the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal pass shader, but since its input is
   * transposed, it will effectively do a vertical pass and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each of
   * the passes. */
  const Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_result(ResultType::Float);
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.weights_result.unbind_as_texture();
  weights.falloffs_result.unbind_as_texture();
  output.unbind_as_image();

  return output;
}

static Result horizontal_pass_cpu(Context &context,
                                  const Result &input,
                                  const int distance,
                                  const int falloff_type)
{
  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will process the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal pass shader, but since its input is
   * transposed, it will effectively do a vertical pass and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each of
   * the passes. */
  const Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_result(ResultType::Float);
  output.allocate_texture(transposed_domain);

  if (distance > 0) {
    morphological_distance_feather_pass<false>(input, weights, output);
  }
  else {
    morphological_distance_feather_pass<true>(input, weights, output);
  }

  return output;
}

static Result horizontal_pass(Context &context,
                              const Result &input,
                              const int distance,
                              const int falloff_type)
{
  if (context.use_gpu()) {
    return horizontal_pass_gpu(context, input, distance, falloff_type);
  }
  return horizontal_pass_cpu(context, input, distance, falloff_type);
}

static void vertical_pass_gpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              Result &output,
                              const int distance,
                              const int falloff_type)
{
  GPUShader *shader = context.get_shader(get_shader_name(distance));
  GPU_shader_bind(shader);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));
  weights.weights_result.bind_as_texture(shader, "weights_tx");
  weights.falloffs_result.bind_as_texture(shader, "falloffs_tx");

  const Domain domain = original_input.domain();
  output.allocate_texture(domain);
  output.bind_as_image(shader, "output_img");

  /* Notice that the domain is transposed, see the note on the horizontal pass function for more
   * information on the reasoning behind this. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

  GPU_shader_unbind();
  horizontal_pass_result.unbind_as_texture();
  weights.weights_result.unbind_as_texture();
  weights.falloffs_result.unbind_as_texture();
  output.unbind_as_image();
}

static void vertical_pass_cpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              Result &output,
                              const int distance,
                              const int falloff_type)
{
  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));

  const Domain domain = original_input.domain();
  output.allocate_texture(domain);

  if (distance > 0) {
    morphological_distance_feather_pass<false>(horizontal_pass_result, weights, output);
  }
  else {
    morphological_distance_feather_pass<true>(horizontal_pass_result, weights, output);
  }
}

static void vertical_pass(Context &context,
                          const Result &original_input,
                          const Result &horizontal_pass_result,
                          Result &output,
                          const int distance,
                          const int falloff_type)
{
  if (context.use_gpu()) {
    vertical_pass_gpu(
        context, original_input, horizontal_pass_result, output, distance, falloff_type);
  }
  else {
    vertical_pass_cpu(
        context, original_input, horizontal_pass_result, output, distance, falloff_type);
  }
}

void morphological_distance_feather(Context &context,
                                    const Result &input,
                                    Result &output,
                                    const int distance,
                                    const int falloff_type)
{
  Result horizontal_pass_result = horizontal_pass(context, input, distance, falloff_type);
  vertical_pass(context, input, horizontal_pass_result, output, distance, falloff_type);
  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
