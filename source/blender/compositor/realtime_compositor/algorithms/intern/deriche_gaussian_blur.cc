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

#include "COM_algorithm_deriche_gaussian_blur.hh"
#include "COM_deriche_gaussian_coefficients.hh"

namespace blender::realtime_compositor {

/* Sum the causal and non causal outputs of the filter and write the sum to the output. This is
 * because the Deriche filter is a parallel interconnection filter, meaning its output is the sum
 * of its causal and non causal filters. The output is expected not to be allocated as it will be
 * allocated internally.
 *
 * The output is allocated and written transposed, that is, with a height equivalent to the width
 * of the input and vice versa. This is done as a performance optimization. The blur pass will
 * blur the image horizontally and write it to the intermediate output transposed. Then the
 * vertical pass will execute the same horizontal blur shader, but since its input is transposed,
 * it will effectively do a vertical blur and write to the output transposed, effectively undoing
 * the transposition in the horizontal pass. This is done to improve spatial cache locality in the
 * shader and to avoid having two separate shaders for each blur pass. */
static void sum_causal_and_non_causal_results(Context &context,
                                              Result &causal_input,
                                              Result &non_causal_input,
                                              Result &output)
{
  GPUShader *shader = context.get_shader("compositor_deriche_gaussian_blur_sum");
  GPU_shader_bind(shader);

  causal_input.bind_as_texture(shader, "causal_input_tx");
  non_causal_input.bind_as_texture(shader, "non_causal_input_tx");

  const Domain domain = causal_input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  causal_input.unbind_as_texture();
  non_causal_input.unbind_as_texture();
  output.unbind_as_image();
}

static void blur_pass(Context &context, Result &input, Result &output, float sigma)
{
  GPUShader *shader = context.get_shader("compositor_deriche_gaussian_blur");
  GPU_shader_bind(shader);

  const DericheGaussianCoefficients &coefficients =
      context.cache_manager().deriche_gaussian_coefficients.get(context, sigma);

  GPU_shader_uniform_4fv(shader,
                         "causal_feedforward_coefficients",
                         float4(coefficients.causal_feedforward_coefficients()));
  GPU_shader_uniform_4fv(shader,
                         "non_causal_feedforward_coefficients",
                         float4(coefficients.non_causal_feedforward_coefficients()));
  GPU_shader_uniform_4fv(
      shader, "feedback_coefficients", float4(coefficients.feedback_coefficients()));
  GPU_shader_uniform_1f(
      shader, "causal_boundary_coefficient", float(coefficients.causal_boundary_coefficient()));
  GPU_shader_uniform_1f(shader,
                        "non_causal_boundary_coefficient",
                        float(coefficients.non_causal_boundary_coefficient()));

  input.bind_as_texture(shader, "input_tx");

  const Domain domain = input.domain();

  Result causal_result = context.create_temporary_result(ResultType::Color);
  causal_result.allocate_texture(domain);
  causal_result.bind_as_image(shader, "causal_output_img");

  Result non_causal_result = context.create_temporary_result(ResultType::Color);
  non_causal_result.allocate_texture(domain);
  non_causal_result.bind_as_image(shader, "non_causal_output_img");

  /* The second dispatch dimension is two dispatches, one for the causal filter and one for the non
   * causal one. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, 2), int2(128, 2));

  GPU_shader_unbind();
  input.unbind_as_texture();
  causal_result.unbind_as_image();
  non_causal_result.unbind_as_image();

  sum_causal_and_non_causal_results(context, causal_result, non_causal_result, output);
  causal_result.release();
  non_causal_result.release();
}

void deriche_gaussian_blur(Context &context, Result &input, Result &output, float2 sigma)
{
  BLI_assert_msg(math::reduce_max(sigma) >= 3.0f,
                 "Deriche filter is slower and less accurate than direct convolution for sigma "
                 "values less 3. Use direct convolution blur instead.");
  BLI_assert_msg(math::reduce_max(sigma) < 32.0f,
                 "Deriche filter is not accurate nor numerically stable for sigma values larger "
                 "than 32. Use Van Vliet filter instead.");

  Result horizontal_pass_result = context.create_temporary_result(ResultType::Color);
  blur_pass(context, input, horizontal_pass_result, sigma.x);
  blur_pass(context, horizontal_pass_result, output, sigma.y);
  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
