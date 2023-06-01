/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_context.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_symmetric_separable_blur.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::realtime_compositor {

static Result horizontal_pass(Context &context,
                              Result &input,
                              float radius,
                              int filter_type,
                              bool extend_bounds,
                              bool gamma_correct)
{
  GPUShader *shader = context.shader_manager().get("compositor_symmetric_separable_blur");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "extend_bounds", extend_bounds);
  GPU_shader_uniform_1b(shader, "gamma_correct_input", gamma_correct);
  GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", false);

  input.bind_as_texture(shader, "input_tx");

  const SymmetricSeparableBlurWeights &weights =
      context.cache_manager().symmetric_separable_blur_weights.get(filter_type, radius);
  weights.bind_as_texture(shader, "weights_tx");

  Domain domain = input.domain();
  if (extend_bounds) {
    domain.size.x += int(math::ceil(radius)) * 2;
  }

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will blur the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal blur shader, but since its input is
   * transposed, it will effectively do a vertical blur and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each blur
   * pass. */
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = Result::Temporary(ResultType::Color, context.texture_pool());
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_as_texture();
  output.unbind_as_image();

  return output;
}

static void vertical_pass(Context &context,
                          Result &original_input,
                          Result &horizontal_pass_result,
                          Result &output,
                          float2 radius,
                          int filter_type,
                          bool extend_bounds,
                          bool gamma_correct)
{
  GPUShader *shader = context.shader_manager().get("compositor_symmetric_separable_blur");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "extend_bounds", extend_bounds);
  GPU_shader_uniform_1b(shader, "gamma_correct_input", false);
  GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", gamma_correct);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const SymmetricSeparableBlurWeights &weights =
      context.cache_manager().symmetric_separable_blur_weights.get(filter_type, radius.y);
  weights.bind_as_texture(shader, "weights_tx");

  Domain domain = original_input.domain();
  if (extend_bounds) {
    /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
    domain.size += int2(math::ceil(radius)) * 2;
  }

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

void symmetric_separable_blur(Context &context,
                              Result &input,
                              Result &output,
                              float2 radius,
                              int filter_type,
                              bool extend_bounds,
                              bool gamma_correct)
{
  Result horizontal_pass_result = horizontal_pass(
      context, input, radius.x, filter_type, extend_bounds, gamma_correct);

  vertical_pass(context,
                input,
                horizontal_pass_result,
                output,
                radius,
                filter_type,
                extend_bounds,
                gamma_correct);

  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
