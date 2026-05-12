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

#include "COM_algorithm_pad.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::compositor {

template<typename T>
static void blur_pass(const Result &input, const Result &weights, Result &output)
{

  /* Notice that the size is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  const int2 size = int2(output.domain().data_size.y, output.domain().data_size.x);
  parallel_for(size, [&](const int2 texel) {
    /* Use float4 for Color types since Color does not support arithmetic. */
    using AccumulateT = std::conditional_t<std::is_same_v<T, Color>, float4, T>;
    AccumulateT accumulated_value = AccumulateT(0);

    /* First, compute the contribution of the center pixel. */
    AccumulateT center_value = AccumulateT(input.load_pixel_extended<T>(texel));
    accumulated_value += center_value * weights.load_pixel<float>(int2(0));

    /* Then, compute the contributions of the pixel to the right and left, noting that the
     * weights texture only stores the weights for the positive half, but since the filter is
     * symmetric, the same weight is used for the negative half and we add both of their
     * contributions. */
    for (int i = 1; i < weights.domain().data_size.x; i++) {
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

static Result blur_pass_gpu(Context &context,
                            const Result &input,
                            Result &output,
                            const float radius,
                            const math::FilterKernel filter_type)
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
  const int2 transposed_domain = int2(domain.data_size.y, domain.data_size.x);

  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.data_size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_as_texture();
  output.unbind_as_image();

  return output;
}

static Result blur_pass_cpu(Context &context,
                            const Result &input,
                            Result &output,
                            const float radius,
                            const math::FilterKernel filter_type)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius);

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The code will
   * blur the image horizontally and write it to the intermediate output transposed. Then the
   * vertical pass will execute the same horizontal blur shader, but since its input is transposed,
   * it will effectively do a vertical blur and write to the output transposed, effectively undoing
   * the transposition in the horizontal pass. This is done to improve spatial cache locality in
   * the shader and to avoid having two separate shaders for each blur pass. */
  const Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.data_size.y, domain.data_size.x);

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

static Result blur_pass(Context &context,
                        const Result &input,
                        Result &output,
                        const float radius,
                        const math::FilterKernel filter_type)
{
  if (context.use_gpu()) {
    return blur_pass_gpu(context, input, output, radius, filter_type);
  }
  return blur_pass_cpu(context, input, output, radius, filter_type);
}

void symmetric_separable_blur(Context &context,
                              const Result &input,
                              Result &output,
                              const float2 &radius,
                              const math::FilterKernel filter_type,
                              const bool extend_bounds)
{
  if (extend_bounds) {
    const int2 padding_size = int2(math::ceil(radius));
    Result padded_input = context.create_result(input.type());
    pad(context, input, padded_input, int2(padding_size.x, 0), PaddingMethod::Zero);

    Result horizontal_pass_result = context.create_result(input.type());
    blur_pass(context, padded_input, horizontal_pass_result, radius.x, filter_type);
    padded_input.release();

    Result padded_horizontal_pass_result = context.create_result(input.type());
    pad(context,
        horizontal_pass_result,
        padded_horizontal_pass_result,
        int2(padding_size.y, 0),
        PaddingMethod::Zero);
    horizontal_pass_result.release();

    blur_pass(context, padded_horizontal_pass_result, output, radius.y, filter_type);
    padded_horizontal_pass_result.release();
  }
  else {
    Result horizontal_pass_result = context.create_result(input.type());
    blur_pass(context, input, horizontal_pass_result, radius.x, filter_type);
    blur_pass(context, horizontal_pass_result, output, radius.y, filter_type);
    horizontal_pass_result.release();
  }
}

}  // namespace blender::compositor
