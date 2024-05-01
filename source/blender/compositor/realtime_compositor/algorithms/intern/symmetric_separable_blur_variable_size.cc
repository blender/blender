/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_symmetric_separable_blur_variable_size.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::realtime_compositor {

static const char *get_blur_shader(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "compositor_symmetric_separable_blur_variable_size_float";
    case ResultType::Float2:
      return "compositor_symmetric_separable_blur_variable_size_float2";
    case ResultType::Vector:
    case ResultType::Color:
      return "compositor_symmetric_separable_blur_variable_size_float4";
    case ResultType::Float3:
      /* GPU module does not support float3 outputs. */
      break;
    case ResultType::Int2:
      /* Blur does not support integer types. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static Result horizontal_pass(
    Context &context, Result &input, Result &radius, int filter_type, int weights_resolution)
{
  GPUShader *shader = context.get_shader(get_blur_shader(input.type()));
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "is_vertical_pass", false);

  input.bind_as_texture(shader, "input_tx");

  const SymmetricSeparableBlurWeights &weights =
      context.cache_manager().symmetric_separable_blur_weights.get(
          context, filter_type, weights_resolution);
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

  Result output = context.create_temporary_result(input.type());
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

static void vertical_pass(Context &context,
                          Result &original_input,
                          Result &horizontal_pass_result,
                          Result &output,
                          Result &radius,
                          int filter_type,
                          int weights_resolution)
{
  GPUShader *shader = context.get_shader(get_blur_shader(original_input.type()));
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "is_vertical_pass", true);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const SymmetricSeparableBlurWeights &weights =
      context.cache_manager().symmetric_separable_blur_weights.get(
          context, filter_type, weights_resolution);
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

void symmetric_separable_blur_variable_size(Context &context,
                                            Result &input,
                                            Result &output,
                                            Result &radius,
                                            int filter_type,
                                            int weights_resolution)
{
  Result horizontal_pass_result = horizontal_pass(
      context, input, radius, filter_type, weights_resolution);
  vertical_pass(
      context, input, horizontal_pass_result, output, radius, filter_type, weights_resolution);
  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
