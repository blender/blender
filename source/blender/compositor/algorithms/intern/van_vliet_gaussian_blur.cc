/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_van_vliet_gaussian_blur.hh"
#include "COM_van_vliet_gaussian_coefficients.hh"

namespace blender::compositor {

#define FILTER_ORDER 2

static void sum_causal_and_non_causal_results_gpu(Context &context,
                                                  const Result &first_causal_input,
                                                  const Result &first_non_causal_input,
                                                  const Result &second_causal_input,
                                                  const Result &second_non_causal_input,
                                                  Result &output)
{
  gpu::Shader *shader = context.get_shader("compositor_van_vliet_gaussian_blur_sum");
  GPU_shader_bind(shader);

  first_causal_input.bind_as_texture(shader, "first_causal_input_tx");
  first_non_causal_input.bind_as_texture(shader, "first_non_causal_input_tx");
  second_causal_input.bind_as_texture(shader, "second_causal_input_tx");
  second_non_causal_input.bind_as_texture(shader, "second_non_causal_input_tx");

  const Domain domain = first_causal_input.domain();
  const Domain transposed_domain = domain.transposed();
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

/* See sum_causal_and_non_causal_results. */
static void sum_causal_and_non_causal_results_cpu(const Result &first_causal_input,
                                                  const Result &first_non_causal_input,
                                                  const Result &second_causal_input,
                                                  const Result &second_non_causal_input,
                                                  Result &output)
{
  const Domain domain = first_causal_input.domain();
  const Domain transposed_domain = domain.transposed();
  output.allocate_texture(transposed_domain);
  parallel_for(domain.size, [&](const int2 texel) {
    /* The Van Vliet filter is a parallel interconnection filter, meaning its output is the sum of
     * all of its causal and non causal filters. */
    float4 filter_output = float4(first_causal_input.load_pixel<Color>(texel)) +
                           float4(first_non_causal_input.load_pixel<Color>(texel)) +
                           float4(second_causal_input.load_pixel<Color>(texel)) +
                           float4(second_non_causal_input.load_pixel<Color>(texel));

    /* Write the color using the transposed texel. See the sum_causal_and_non_causal_results method
     * for more information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), Color(filter_output));
  });
}

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
                                              const Result &first_causal_input,
                                              const Result &first_non_causal_input,
                                              const Result &second_causal_input,
                                              const Result &second_non_causal_input,
                                              Result &output)
{
  if (context.use_gpu()) {
    sum_causal_and_non_causal_results_gpu(context,
                                          first_causal_input,
                                          first_non_causal_input,
                                          second_causal_input,
                                          second_non_causal_input,
                                          output);
  }
  else {
    sum_causal_and_non_causal_results_cpu(first_causal_input,
                                          first_non_causal_input,
                                          second_causal_input,
                                          second_non_causal_input,
                                          output);
  }
}

static void blur_pass_gpu(Context &context,
                          const Result &input,
                          Result &first_causal_result,
                          Result &first_non_causal_result,
                          Result &second_causal_result,
                          Result &second_non_causal_result,
                          const float sigma)
{
  gpu::Shader *shader = context.get_shader("compositor_van_vliet_gaussian_blur");
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

  first_causal_result.allocate_texture(domain);
  first_causal_result.bind_as_image(shader, "first_causal_output_img");

  first_non_causal_result.allocate_texture(domain);
  first_non_causal_result.bind_as_image(shader, "first_non_causal_output_img");

  second_causal_result.allocate_texture(domain);
  second_causal_result.bind_as_image(shader, "second_causal_output_img");

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
}

static void blur_pass_cpu(Context &context,
                          const Result &input,
                          Result &first_causal_output,
                          Result &first_non_causal_output,
                          Result &second_causal_output,
                          Result &second_non_causal_output,
                          const float sigma)
{
  const VanVlietGaussianCoefficients &coefficients =
      context.cache_manager().van_vliet_gaussian_coefficients.get(context, sigma);

  const float2 first_feedback_coefficients = float2(coefficients.first_feedback_coefficients());
  const float2 first_causal_feedforward_coefficients = float2(
      coefficients.first_causal_feedforward_coefficients());
  const float2 first_non_causal_feedforward_coefficients = float2(
      coefficients.first_non_causal_feedforward_coefficients());
  const float2 second_feedback_coefficients = float2(coefficients.second_feedback_coefficients());
  const float2 second_causal_feedforward_coefficients = float2(
      coefficients.second_causal_feedforward_coefficients());
  const float2 second_non_causal_feedforward_coefficients = float2(
      coefficients.second_non_causal_feedforward_coefficients());
  const float first_causal_boundary_coefficient = float(
      coefficients.first_causal_boundary_coefficient());
  const float first_non_causal_boundary_coefficient = float(
      coefficients.first_non_causal_boundary_coefficient());
  const float second_causal_boundary_coefficient = float(
      coefficients.second_causal_boundary_coefficient());
  const float second_non_causal_boundary_coefficient = float(
      coefficients.second_non_causal_boundary_coefficient());

  const Domain domain = input.domain();
  first_causal_output.allocate_texture(domain);
  first_non_causal_output.allocate_texture(domain);
  second_causal_output.allocate_texture(domain);
  second_non_causal_output.allocate_texture(domain);

  /* The first dispatch dimension is 4 dispatches, one for the first causal filter, one for the
   * first non causal filter, one for the second causal filter, and one for the second non causal
   * filter. */
  const int2 parallel_for_size = int2(4, domain.size.y);
  /* Blur the input horizontally by applying a fourth order IIR filter approximating a Gaussian
   * filter using Van Vliet's design method. This is based on the following paper:
   *
   *   Van Vliet, Lucas J., Ian T. Young, and Piet W. Verbeek. "Recursive Gaussian derivative
   *   filters." Proceedings. Fourteenth International Conference on Pattern Recognition (Cat. No.
   *   98EX170). Vol. 1. IEEE, 1998.
   *
   * We decomposed the filter into two second order filters, so we actually run four filters per
   * row in parallel, one for the first causal filter, one for the first non causal filter, one for
   * the second causal filter, and one for the second non causal filter, storing the result of each
   * separately. See the VanVlietGaussianCoefficients class and the implementation for more
   * information. */
  parallel_for(parallel_for_size, [&](const int2 invocation) {
    /* The shader runs parallel across rows but serially across columns. */
    int y = invocation.y;
    int width = input.domain().size.x;

    /* The second dispatch dimension is four dispatches:
     *
     *   0 -> First causal filter.
     *   1 -> First non causal filter.
     *   2 -> Second causal filter.
     *   3 -> Second non causal filter.
     *
     * We detect causality by even numbers. */
    bool is_causal = invocation.x % 2 == 0;
    float2 first_feedforward_coefficients = is_causal ? first_causal_feedforward_coefficients :
                                                        first_non_causal_feedforward_coefficients;
    float first_boundary_coefficient = is_causal ? first_causal_boundary_coefficient :
                                                   first_non_causal_boundary_coefficient;
    float2 second_feedforward_coefficients = is_causal ?
                                                 second_causal_feedforward_coefficients :
                                                 second_non_causal_feedforward_coefficients;
    float second_boundary_coefficient = is_causal ? second_causal_boundary_coefficient :
                                                    second_non_causal_boundary_coefficient;
    /* And we detect the filter by order. */
    bool is_first_filter = invocation.x < 2;
    float2 feedforward_coefficients = is_first_filter ? first_feedforward_coefficients :
                                                        second_feedforward_coefficients;
    float2 feedback_coefficients = is_first_filter ? first_feedback_coefficients :
                                                     second_feedback_coefficients;
    float boundary_coefficient = is_first_filter ? first_boundary_coefficient :
                                                   second_boundary_coefficient;

    /* Create an array that holds the last FILTER_ORDER inputs along with the current input. The
     * current input is at index 0 and the oldest input is at index FILTER_ORDER. We assume Neumann
     * boundary condition, so we initialize all inputs by the boundary pixel. */
    int2 boundary_texel = is_causal ? int2(0, y) : int2(width - 1, y);
    float4 input_boundary = float4(input.load_pixel<Color>(boundary_texel));
    float4 inputs[FILTER_ORDER + 1] = {input_boundary, input_boundary, input_boundary};

    /* Create an array that holds the last FILTER_ORDER outputs along with the current output. The
     * current output is at index 0 and the oldest output is at index FILTER_ORDER. We assume
     * Neumann boundary condition, so we initialize all outputs by the boundary pixel multiplied by
     * the boundary coefficient. See the VanVlietGaussianCoefficients class for more information on
     * the boundary handing. */
    float4 output_boundary = input_boundary * boundary_coefficient;
    float4 outputs[FILTER_ORDER + 1] = {output_boundary, output_boundary, output_boundary};

    for (int x = 0; x < width; x++) {
      /* Run forward across rows for the causal filter and backward for the non causal filter. */
      int2 texel = is_causal ? int2(x, y) : int2(width - 1 - x, y);
      inputs[0] = float4(input.load_pixel<Color>(texel));

      /* Compute the filter based on its difference equation, this is not in the Van Vliet paper
       * because the filter was decomposed, but it is essentially similar to Equation (28) for the
       * causal filter or Equation (29) for the non causal filter in Deriche's paper, except it is
       * second order, not fourth order.
       *
       *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss.
       * INRIA, 1993.
       *
       * The only difference is that the non causal filter ignores the current value and starts
       * from the previous input, as can be seen in the subscript of the first input term in both
       * equations. So add one while indexing the non causal inputs. */
      outputs[0] = float4(0.0f);
      int first_input_index = is_causal ? 0 : 1;
      for (int i = 0; i < FILTER_ORDER; i++) {
        outputs[0] += feedforward_coefficients[i] * inputs[first_input_index + i];
        outputs[0] -= feedback_coefficients[i] * outputs[i + 1];
      }

      /* Store the causal and non causal outputs of each of the two filters independently, then sum
       * them in a separate shader dispatch for better parallelism. */
      if (is_causal) {
        if (is_first_filter) {
          first_causal_output.store_pixel(texel, Color(outputs[0]));
        }
        else {
          second_causal_output.store_pixel(texel, Color(outputs[0]));
        }
      }
      else {
        if (is_first_filter) {
          first_non_causal_output.store_pixel(texel, Color(outputs[0]));
        }
        else {
          second_non_causal_output.store_pixel(texel, Color(outputs[0]));
        }
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
  Result first_causal_result = context.create_result(ResultType::Color);
  Result first_non_causal_result = context.create_result(ResultType::Color);
  Result second_causal_result = context.create_result(ResultType::Color);
  Result second_non_causal_result = context.create_result(ResultType::Color);

  if (context.use_gpu()) {
    blur_pass_gpu(context,
                  input,
                  first_causal_result,
                  first_non_causal_result,
                  second_causal_result,
                  second_non_causal_result,
                  sigma);
  }
  else {
    blur_pass_cpu(context,
                  input,
                  first_causal_result,
                  first_non_causal_result,
                  second_causal_result,
                  second_non_causal_result,
                  sigma);
  }

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

void van_vliet_gaussian_blur(Context &context,
                             const Result &input,
                             Result &output,
                             const float2 &sigma)
{
  BLI_assert_msg(math::reduce_max(sigma) >= 32.0f,
                 "Van Vliet filter is less accurate for sigma values less than 32. Use Deriche "
                 "filter instead or direct convolution instead.");

  Result horizontal_pass_result = context.create_result(ResultType::Color);
  blur_pass(context, input, horizontal_pass_result, sigma.x);
  blur_pass(context, horizontal_pass_result, output, sigma.y);
  horizontal_pass_result.release();
}

}  // namespace blender::compositor
