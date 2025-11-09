/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_deriche_gaussian_blur.hh"
#include "COM_deriche_gaussian_coefficients.hh"

namespace blender::compositor {

#define FILTER_ORDER 4

/* See sum_causal_and_non_causal_results. */
static void sum_causal_and_non_causal_results_gpu(Context &context,
                                                  const Result &causal_input,
                                                  const Result &non_causal_input,
                                                  Result &output)
{
  gpu::Shader *shader = context.get_shader("compositor_deriche_gaussian_blur_sum");
  GPU_shader_bind(shader);

  causal_input.bind_as_texture(shader, "causal_input_tx");
  non_causal_input.bind_as_texture(shader, "non_causal_input_tx");

  const Domain domain = causal_input.domain();
  const Domain transposed_domain = domain.transposed();
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  causal_input.unbind_as_texture();
  non_causal_input.unbind_as_texture();
  output.unbind_as_image();
}

/* See sum_causal_and_non_causal_results. */
static void sum_causal_and_non_causal_results_cpu(const Result &causal_input,
                                                  const Result &non_causal_input,
                                                  Result &output)
{
  const Domain domain = causal_input.domain();
  const Domain transposed_domain = domain.transposed();
  output.allocate_texture(transposed_domain);

  parallel_for(domain.size, [&](const int2 texel) {
    /* The Deriche filter is a parallel interconnection filter, meaning its output is the sum of
     * its causal and non causal filters. */
    float4 filter_output = float4(causal_input.load_pixel<Color>(texel)) +
                           float4(non_causal_input.load_pixel<Color>(texel));

    /* Write the color using the transposed texel. See the sum_causal_and_non_causal_results method
     * in the deriche_gaussian_blur.cc file for more information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), Color(filter_output));
  });
}

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
                                              const Result &causal_input,
                                              const Result &non_causal_input,
                                              Result &output)
{
  if (context.use_gpu()) {
    sum_causal_and_non_causal_results_gpu(context, causal_input, non_causal_input, output);
  }
  else {
    sum_causal_and_non_causal_results_cpu(causal_input, non_causal_input, output);
  }
}

static void blur_pass_gpu(Context &context,
                          const Result &input,
                          Result &causal_result,
                          Result &non_causal_result,
                          const float sigma)
{
  gpu::Shader *shader = context.get_shader("compositor_deriche_gaussian_blur");
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
  causal_result.allocate_texture(domain);
  non_causal_result.allocate_texture(domain);
  causal_result.bind_as_image(shader, "causal_output_img");
  non_causal_result.bind_as_image(shader, "non_causal_output_img");

  /* The second dispatch dimension is two dispatches, one for the causal filter and one for the non
   * causal one. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, 2), int2(128, 2));

  GPU_shader_unbind();
  input.unbind_as_texture();
  causal_result.unbind_as_image();
  non_causal_result.unbind_as_image();
}

static void blur_pass_cpu(Context &context,
                          const Result &input,
                          Result &causal_output,
                          Result &non_causal_output,
                          const float sigma)
{
  const DericheGaussianCoefficients &coefficients =
      context.cache_manager().deriche_gaussian_coefficients.get(context, sigma);

  const float4 causal_feedforward_coefficients = float4(
      coefficients.causal_feedforward_coefficients());
  const float4 non_causal_feedforward_coefficients = float4(
      coefficients.non_causal_feedforward_coefficients());
  const float4 feedback_coefficients = float4(coefficients.feedback_coefficients());
  const float causal_boundary_coefficient = float(coefficients.causal_boundary_coefficient());
  const float non_causal_boundary_coefficient = float(
      coefficients.non_causal_boundary_coefficient());

  const Domain domain = input.domain();
  causal_output.allocate_texture(domain);
  non_causal_output.allocate_texture(domain);

  /* The first dispatch dimension is two dispatches, one for the causal filter and one for the non
   * causal one. */
  const int2 parallel_for_size = int2(2, domain.size.y);
  /* Blur the input horizontally by applying a fourth order IIR filter approximating a Gaussian
   * filter using Deriche's design method. This is based on the following paper:
   *
   *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
   *   1993.
   *
   * We run two filters per row in parallel, one for the causal filter and one for the non causal
   * filter, storing the result of each separately. See the DericheGaussianCoefficients class and
   * the implementation for more information. */
  parallel_for(parallel_for_size, [&](const int2 invocation) {
    /* The code runs parallel across rows but serially across columns. */
    int y = invocation.y;
    int width = input.domain().size.x;

    /* The second dispatch dimension is two dispatches, one for the causal filter and one for the
     * non causal one. */
    bool is_causal = invocation.x == 0;
    float4 feedforward_coefficients = is_causal ? causal_feedforward_coefficients :
                                                  non_causal_feedforward_coefficients;
    float boundary_coefficient = is_causal ? causal_boundary_coefficient :
                                             non_causal_boundary_coefficient;

    /* Create an array that holds the last FILTER_ORDER inputs along with the current input. The
     * current input is at index 0 and the oldest input is at index FILTER_ORDER. We assume Neumann
     * boundary condition, so we initialize all inputs by the boundary pixel. */
    int2 boundary_texel = is_causal ? int2(0, y) : int2(width - 1, y);
    float4 input_boundary = float4(input.load_pixel<Color>(boundary_texel));
    float4 inputs[FILTER_ORDER + 1] = {
        input_boundary, input_boundary, input_boundary, input_boundary, input_boundary};

    /* Create an array that holds the last FILTER_ORDER outputs along with the current output. The
     * current output is at index 0 and the oldest output is at index FILTER_ORDER. We assume
     * Neumann boundary condition, so we initialize all outputs by the boundary pixel multiplied by
     * the boundary coefficient. See the DericheGaussianCoefficients class for more information on
     * the boundary handing. */
    float4 output_boundary = input_boundary * boundary_coefficient;
    float4 outputs[FILTER_ORDER + 1] = {
        output_boundary, output_boundary, output_boundary, output_boundary, output_boundary};

    for (int x = 0; x < width; x++) {
      /* Run forward across rows for the causal filter and backward for the non causal filter. */
      int2 texel = is_causal ? int2(x, y) : int2(width - 1 - x, y);
      inputs[0] = float4(input.load_pixel<Color>(texel));

      /* Compute Equation (28) for the causal filter or Equation (29) for the non causal filter.
       * The only difference is that the non causal filter ignores the current value and starts
       * from the previous input, as can be seen in the subscript of the first input term in both
       * equations. So add one while indexing the non causal inputs. */
      outputs[0] = float4(0.0f);
      int first_input_index = is_causal ? 0 : 1;
      for (int i = 0; i < FILTER_ORDER; i++) {
        outputs[0] += feedforward_coefficients[i] * inputs[first_input_index + i];
        outputs[0] -= feedback_coefficients[i] * outputs[i + 1];
      }

      /* Store the causal and non causal outputs independently, then sum them in a separate shader
       * dispatch for better parallelism. */
      if (is_causal) {
        causal_output.store_pixel(texel, Color(outputs[0]));
      }
      else {
        non_causal_output.store_pixel(texel, Color(outputs[0]));
      }

      /* Shift the inputs temporally by one. The oldest input is discarded, while the current input
       * will retain its value but will be overwritten with the new current value in the next
       * iteration. */
      for (int i = FILTER_ORDER; i >= 1; i--) {
        inputs[i] = inputs[i - 1];
      }

      /* Shift the outputs temporally by one. The oldest output is discarded, while the current
       * output will retain its value but will be overwritten with the new current value in the
       * next iteration. */
      for (int i = FILTER_ORDER; i >= 1; i--) {
        outputs[i] = outputs[i - 1];
      }
    }
  });
}

static void blur_pass(Context &context, const Result &input, Result &output, const float sigma)
{
  Result causal_result = context.create_result(ResultType::Color);
  Result non_causal_result = context.create_result(ResultType::Color);

  if (context.use_gpu()) {
    blur_pass_gpu(context, input, causal_result, non_causal_result, sigma);
  }
  else {
    blur_pass_cpu(context, input, causal_result, non_causal_result, sigma);
  }

  sum_causal_and_non_causal_results(context, causal_result, non_causal_result, output);
  causal_result.release();
  non_causal_result.release();
}

void deriche_gaussian_blur(Context &context,
                           const Result &input,
                           Result &output,
                           const float2 &sigma)
{
  BLI_assert_msg(math::reduce_max(sigma) >= 3.0f,
                 "Deriche filter is slower and less accurate than direct convolution for sigma "
                 "values less 3. Use direct convolution blur instead.");
  BLI_assert_msg(math::reduce_max(sigma) < 32.0f,
                 "Deriche filter is not accurate nor numerically stable for sigma values larger "
                 "than 32. Use Van Vliet filter instead.");

  Result horizontal_pass_result = context.create_result(ResultType::Color);
  blur_pass(context, input, horizontal_pass_result, sigma.x);
  blur_pass(context, horizontal_pass_result, output, sigma.y);
  horizontal_pass_result.release();
}

}  // namespace blender::compositor
