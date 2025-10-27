/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <type_traits>

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_symmetric_separable_blur.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::compositor {

template<typename T>
static void blur_pass(const Result &input, const Result &weights, Result &output)
{

  /* Notice that the size is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  const int2 size = int2(output.domain().size.y, output.domain().size.x);
  parallel_for(size, [&](const int2 texel) {
    /* Use float4 for Color types since Color does not support arithmetics. */
    using AccumulateT = std::conditional_t<std::is_same_v<T, Color>, float4, T>;
    AccumulateT accumulated_value = AccumulateT(0);

    /* First, compute the contribution of the center pixel. */
    AccumulateT center_value = AccumulateT(input.load_pixel_extended<T>(texel));
    accumulated_value += center_value * weights.load_pixel<float>(int2(0));

    /* Then, compute the contributions of the pixel to the right and left, noting that the
     * weights texture only stores the weights for the positive half, but since the filter is
     * symmetric, the same weight is used for the negative half and we add both of their
     * contributions. */
    for (int i = 1; i < weights.domain().size.x; i++) {
      float weight = weights.load_pixel<float>(int2(i, 0));
      accumulated_value += AccumulateT(input.load_pixel_extended<T>(texel + int2(i, 0))) * weight;
      accumulated_value += AccumulateT(input.load_pixel_extended<T>(texel + int2(-i, 0))) * weight;
    }

    /* Write the color using the transposed texel. See the horizontal_pass method for more
     * information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), T(accumulated_value));
  });
}

static const char *get_blur_shader(const ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "compositor_symmetric_separable_blur_float";
    case ResultType::Float4:
    case ResultType::Color:
      return "compositor_symmetric_separable_blur_float4";
    default:
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static Result horizontal_pass_gpu(Context &context,
                                  const Result &input,
                                  const float radius,
                                  const int filter_type)
{
  gpu::Shader *shader = context.get_shader(get_blur_shader(input.type()));
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius);
  weights.bind_as_texture(shader, "weights_tx");

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
  output.unbind_as_image();

  return output;
}

static Result horizontal_pass_cpu(Context &context,
                                  const Result &input,
                                  const float radius,
                                  const int filter_type)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius);

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will blur the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal blur shader, but since its input is
   * transposed, it will effectively do a vertical blur and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each blur
   * pass. */
  const Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_result(input.type());
  output.allocate_texture(transposed_domain);

  switch (input.type()) {
    case ResultType::Float:
      blur_pass<float>(input, weights, output);
      break;
    case ResultType::Float4:
      blur_pass<float4>(input, weights, output);
      break;
    case ResultType::Color:
      blur_pass<Color>(input, weights, output);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  return output;
}

static Result horizontal_pass(Context &context,
                              const Result &input,
                              const float radius,
                              const int filter_type)
{
  if (context.use_gpu()) {
    return horizontal_pass_gpu(context, input, radius, filter_type);
  }
  return horizontal_pass_cpu(context, input, radius, filter_type);
}

static void vertical_pass_gpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              Result &output,
                              const float2 &radius,
                              const int filter_type)
{
  gpu::Shader *shader = context.get_shader(get_blur_shader(original_input.type()));
  GPU_shader_bind(shader);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius.y);
  weights.bind_as_texture(shader, "weights_tx");

  const Domain domain = original_input.domain();
  output.allocate_texture(domain);
  output.bind_as_image(shader, "output_img");

  /* Notice that the domain is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

  GPU_shader_unbind();
  horizontal_pass_result.unbind_as_texture();
  output.unbind_as_image();
  weights.unbind_as_texture();
}

static void vertical_pass_cpu(Context &context,
                              const Result &original_input,
                              const Result &horizontal_pass_result,
                              Result &output,
                              const float2 &radius,
                              const int filter_type)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius.y);

  output.allocate_texture(original_input.domain());

  switch (original_input.type()) {
    case ResultType::Float:
      blur_pass<float>(horizontal_pass_result, weights, output);
      break;
    case ResultType::Float4:
      blur_pass<float4>(horizontal_pass_result, weights, output);
      break;
    case ResultType::Color:
      blur_pass<Color>(horizontal_pass_result, weights, output);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

static void vertical_pass(Context &context,
                          const Result &original_input,
                          const Result &horizontal_pass_result,
                          Result &output,
                          const float2 &radius,
                          const int filter_type)
{
  if (context.use_gpu()) {
    vertical_pass_gpu(
        context, original_input, horizontal_pass_result, output, radius, filter_type);
  }
  else {
    vertical_pass_cpu(
        context, original_input, horizontal_pass_result, output, radius, filter_type);
  }
}

void symmetric_separable_blur(Context &context,
                              const Result &input,
                              Result &output,
                              const float2 &radius,
                              const int filter_type)
{
  Result horizontal_pass_result = horizontal_pass(context, input, radius.x, filter_type);
  vertical_pass(context, input, horizontal_pass_result, output, radius, filter_type);
  horizontal_pass_result.release();
}

}  // namespace blender::compositor
