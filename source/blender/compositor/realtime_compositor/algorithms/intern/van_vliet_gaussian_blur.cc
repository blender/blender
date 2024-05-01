/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_van_vliet_gaussian_blur.hh"
#include "COM_van_vliet_gaussian_coefficients.hh"

namespace blender::realtime_compositor {

/* Sum all four of the causal and non causal outputs of the first and second filters and write the
 * sum to the output. This is because the Van Vliet filter is implemented as a bank of 2 parallel
 * second order filters, meaning its output is the sum of the causal and non causal filters of both
 * filters. The output is expected not to be allocated as it will be allocated internally.
 *
 * The output is allocated and written transposed, that is, with a height equivalent to the width
 * of the input and vice versa. This is done as a performance optimization. The blur pass will
 * blur the image horizontally and write it to the intermediate output transposed. Then the
 * vertical pass will execute the same horizontal blur shader, but since its input is transposed,
 * it will effectively do a vertical blur and write to the output transposed, effectively undoing
 * the transposition in the horizontal pass. This is done to improve spatial cache locality in the
 * shader and to avoid having two separate shaders for each blur pass. */
static void sum_causal_and_non_causal_results(Context &context,
                                              Result &first_causal_input,
                                              Result &first_non_causal_input,
                                              Result &second_causal_input,
                                              Result &second_non_causal_input,
                                              Result &output)
{
  GPUShader *shader = context.get_shader("compositor_van_vliet_gaussian_blur_sum");
  GPU_shader_bind(shader);

  first_causal_input.bind_as_texture(shader, "first_causal_input_tx");
  first_non_causal_input.bind_as_texture(shader, "first_non_causal_input_tx");
  second_causal_input.bind_as_texture(shader, "second_causal_input_tx");
  second_non_causal_input.bind_as_texture(shader, "second_non_causal_input_tx");

  const Domain domain = first_causal_input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  first_causal_input.unbind_as_texture();
  first_non_causal_input.unbind_as_texture();
  second_causal_input.unbind_as_texture();
  second_non_causal_input.unbind_as_texture();
  output.unbind_as_image();
}

static void blur_pass(Context &context, Result &input, Result &output, float sigma)
{
  GPUShader *shader = context.get_shader("compositor_van_vliet_gaussian_blur");
  GPU_shader_bind(shader);

  const VanVlietGaussianCoefficients &coefficients =
      context.cache_manager().van_vliet_gaussian_coefficients.get(context, sigma);

  GPU_shader_uniform_2fv(
      shader, "first_feedback_coefficients", float2(coefficients.first_feedback_coefficients()));
  GPU_shader_uniform_2fv(shader,
                         "first_causal_feedforward_coefficients",
                         float2(coefficients.first_causal_feedforward_coefficients()));
  GPU_shader_uniform_2fv(shader,
                         "first_non_causal_feedforward_coefficients",
                         float2(coefficients.first_non_causal_feedforward_coefficients()));
  GPU_shader_uniform_2fv(
      shader, "second_feedback_coefficients", float2(coefficients.second_feedback_coefficients()));
  GPU_shader_uniform_2fv(shader,
                         "second_causal_feedforward_coefficients",
                         float2(coefficients.second_causal_feedforward_coefficients()));
  GPU_shader_uniform_2fv(shader,
                         "second_non_causal_feedforward_coefficients",
                         float2(coefficients.second_non_causal_feedforward_coefficients()));
  GPU_shader_uniform_1f(shader,
                        "first_causal_boundary_coefficient",
                        float(coefficients.first_causal_boundary_coefficient()));
  GPU_shader_uniform_1f(shader,
                        "first_non_causal_boundary_coefficient",
                        float(coefficients.first_non_causal_boundary_coefficient()));
  GPU_shader_uniform_1f(shader,
                        "second_causal_boundary_coefficient",
                        float(coefficients.second_causal_boundary_coefficient()));
  GPU_shader_uniform_1f(shader,
                        "second_non_causal_boundary_coefficient",
                        float(coefficients.second_non_causal_boundary_coefficient()));

  input.bind_as_texture(shader, "input_tx");

  const Domain domain = input.domain();

  Result first_causal_result = context.create_temporary_result(ResultType::Color);
  first_causal_result.allocate_texture(domain);
  first_causal_result.bind_as_image(shader, "first_causal_output_img");

  Result first_non_causal_result = context.create_temporary_result(ResultType::Color);
  first_non_causal_result.allocate_texture(domain);
  first_non_causal_result.bind_as_image(shader, "first_non_causal_output_img");

  Result second_causal_result = context.create_temporary_result(ResultType::Color);
  second_causal_result.allocate_texture(domain);
  second_causal_result.bind_as_image(shader, "second_causal_output_img");

  Result second_non_causal_result = context.create_temporary_result(ResultType::Color);
  second_non_causal_result.allocate_texture(domain);
  second_non_causal_result.bind_as_image(shader, "second_non_causal_output_img");

  /* The second dispatch dimension is 4 dispatches, one for the first causal filter, one for the
   * first non causal filter, one for the second causal filter, and one for the second non causal
   * filter. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, 4), int2(64, 4));

  GPU_shader_unbind();
  input.unbind_as_texture();
  first_causal_result.unbind_as_image();
  first_non_causal_result.unbind_as_image();
  second_causal_result.unbind_as_image();
  second_non_causal_result.unbind_as_image();

  sum_causal_and_non_causal_results(context,
                                    first_causal_result,
                                    first_non_causal_result,
                                    second_causal_result,
                                    second_non_causal_result,
                                    output);
  first_causal_result.release();
  first_non_causal_result.release();
  second_causal_result.release();
  second_non_causal_result.release();
}

void van_vliet_gaussian_blur(Context &context, Result &input, Result &output, float2 sigma)
{
  BLI_assert_msg(math::reduce_max(sigma) >= 32.0f,
                 "Van Vliet filter is less accurate for sigma values less than 32. Use Deriche "
                 "filter instead or direct convolution instead.");

  Result horizontal_pass_result = context.create_temporary_result(ResultType::Color);
  blur_pass(context, input, horizontal_pass_result, sigma.x);
  blur_pass(context, horizontal_pass_result, output, sigma.y);
  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
