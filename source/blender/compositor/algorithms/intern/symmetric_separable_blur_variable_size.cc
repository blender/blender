/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_symmetric_separable_blur_variable_size.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::compositor {

static void blur_pass(const Result &input,
                      const Result &radius_input,
                      const Result &weights,
                      Result &output,
                      const bool is_vertical_pass)
{
  /* Notice that the size is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  const int2 size = int2(output.domain().size.y, output.domain().size.x);
  parallel_for(size, [&](const int2 texel) {
    float accumulated_weight = 0.0f;
    float4 accumulated_color = float4(0.0f);

    /* First, compute the contribution of the center pixel. */
    float4 center_color = float4(input.load_pixel<Color>(texel));
    float center_weight = weights.load_pixel<float>(int2(0));
    accumulated_color += center_color * center_weight;
    accumulated_weight += center_weight;

    /* The dispatch domain is transposed in the vertical pass, so make sure to reverse transpose
     * the texel coordinates when loading the radius. See the horizontal_pass function for more
     * information. */
    int radius = int(
        radius_input.load_pixel<float>(is_vertical_pass ? int2(texel.y, texel.x) : texel));

    /* Then, compute the contributions of the pixel to the right and left, noting that the
     * weights texture only stores the weights for the positive half, but since the filter is
     * symmetric, the same weight is used for the negative half and we add both of their
     * contributions. */
    for (int i = 1; i <= radius; i++) {
      /* Add 0.5 to evaluate at the center of the pixels. */
      float weight =
          weights.sample_bilinear_extended(float2((float(i) + 0.5f) / float(radius + 1), 0.0f)).x;
      accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(i, 0))) * weight;
      accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-i, 0))) * weight;
      accumulated_weight += weight * 2.0f;
    }

    /* Write the color using the transposed texel. See the horizontal_pass_cpu function for more
     * information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), Color(accumulated_color / accumulated_weight));
  });
}

static Result horizontal_pass_gpu(Context &context,
                                  const Result &input,
                                  const Result &radius,
                                  const int weights_resolution,
                                  const int filter_type)
{
  gpu::Shader *shader = context.get_shader("compositor_symmetric_separable_blur_variable_size");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "is_vertical_pass", false);

  input.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, weights_resolution);
  GPU_texture_filter_mode(weights, true);
  GPU_texture_extend_mode(weights, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  weights.bind_as_texture(shader, "weights_tx");

  radius.bind_as_texture(shader, "radius_tx");

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will blur the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal blur shader, but since its input is
   * transposed, it will effectively do a vertical blur and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each blur
   * pass. */
  Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_result(input.type());
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_as_texture();
  radius.unbind_as_texture();
  output.unbind_as_image();

  return output;
}

static Result horizontal_pass_cpu(Context &context,
                                  const Result &input,
                                  const Result &radius,
                                  const int weights_resolution,
                                  const int filter_type)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, weights_resolution);

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will blur the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal blur shader, but since its input is
   * transposed, it will effectively do a vertical blur and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each blur
   * pass. */
  Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_result(input.type());
  output.allocate_texture(transposed_domain);

  blur_pass(input, radius, weights, output, false);

  return output;
}

static Result horizontal_pass(Context &context,
                              const Result &input,
                              const Result &radius,
                              const int weights_resolution,
                              const int filter_type)
{
  if (context.use_gpu()) {
    return horizontal_pass_gpu(context, input, radius, weights_resolution, filter_type);
  }
  return horizontal_pass_cpu(context, input, radius, weights_resolution, filter_type);
}

static void vertical_pass_gpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              const Result &radius,
                              Result &output,
                              const int weights_resolution,
                              const int filter_type)
{
  gpu::Shader *shader = context.get_shader("compositor_symmetric_separable_blur_variable_size");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "is_vertical_pass", true);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, weights_resolution);
  GPU_texture_filter_mode(weights, true);
  GPU_texture_extend_mode(weights, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  weights.bind_as_texture(shader, "weights_tx");

  radius.bind_as_texture(shader, "radius_tx");

  Domain domain = original_input.domain();
  output.allocate_texture(domain);
  output.bind_as_image(shader, "output_img");

  /* Notice that the domain is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

  GPU_shader_unbind();
  horizontal_pass_result.unbind_as_texture();
  output.unbind_as_image();
  weights.unbind_as_texture();
  radius.unbind_as_texture();
}

static void vertical_pass_cpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              const Result &radius,
                              Result &output,
                              const int weights_resolution,
                              const int filter_type)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, weights_resolution);

  Domain domain = original_input.domain();
  output.allocate_texture(domain);

  blur_pass(horizontal_pass_result, radius, weights, output, true);
}

static void vertical_pass(Context &context,
                          const Result &original_input,
                          const Result &horizontal_pass_result,
                          const Result &radius,
                          Result &output,
                          const int weights_resolution,
                          const int filter_type)
{
  if (context.use_gpu()) {
    vertical_pass_gpu(context,
                      original_input,
                      horizontal_pass_result,
                      radius,
                      output,
                      weights_resolution,
                      filter_type);
  }
  else {
    vertical_pass_cpu(context,
                      original_input,
                      horizontal_pass_result,
                      radius,
                      output,
                      weights_resolution,
                      filter_type);
  }
}

void symmetric_separable_blur_variable_size(Context &context,
                                            const Result &input,
                                            const Result &radius,
                                            Result &output,
                                            const int weights_resolution,
                                            const int filter_type)
{
  BLI_assert(input.type() == ResultType::Color);

  Result horizontal_pass_result = horizontal_pass(
      context, input, radius, weights_resolution, filter_type);
  vertical_pass(
      context, input, horizontal_pass_result, radius, output, weights_resolution, filter_type);
  horizontal_pass_result.release();
}

}  // namespace blender::compositor
