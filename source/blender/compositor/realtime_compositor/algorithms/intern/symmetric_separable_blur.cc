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

#include "COM_algorithm_symmetric_separable_blur.hh"

#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::realtime_compositor {

/* Preprocess the input of the blur filter by squaring it in its alpha straight form, assuming
 * the given color is alpha pre-multiplied. */
static float4 gamma_correct_blur_input(const float4 &color)
{
  float alpha = color.w > 0.0f ? color.w : 1.0f;
  float3 corrected_color = math::square(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
  return float4(corrected_color, color.w);
}

/* Postprocess the output of the blur filter by taking its square root it in its alpha straight
 * form, assuming the given color is alpha pre-multiplied. This essential undoes the processing
 * done by the gamma_correct_blur_input function. */
static float4 gamma_uncorrect_blur_output(const float4 &color)
{
  float alpha = color.w > 0.0f ? color.w : 1.0f;
  float3 uncorrected_color = math::sqrt(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
  return float4(uncorrected_color, color.w);
}

static void blur_pass(const Result &input,
                      const Result &weights,
                      Result &output,
                      const bool extend_bounds,
                      const bool gamma_correct_input,
                      const bool gamma_uncorrect_output)
{
  /* Loads the input color of the pixel at the given texel. If gamma correction is enabled, the
   * color is gamma corrected. If bounds are extended, then the input is treated as padded by a
   * blur size amount of pixels of zero color, and the given texel is assumed to be in the space of
   * the image after padding. So we offset the texel by the blur radius amount and fallback to a
   * zero color if it is out of bounds. For instance, if the input is padded by 5 pixels to the
   * left of the image, the first 5 pixels should be out of bounds and thus zero, hence the
   * introduced offset. */
  auto load_input = [&](const int2 texel) {
    float4 color;
    if (extend_bounds) {
      /* Notice that we subtract 1 because the weights result have an extra center weight, see the
       * SymmetricBlurWeights class for more information. */
      int2 blur_radius = weights.domain().size - 1;
      color = input.load_pixel_fallback(texel - blur_radius, float4(0.0f));
    }
    else {
      color = input.load_pixel_extended(texel);
    }

    if (gamma_correct_input) {
      color = gamma_correct_blur_input(color);
    }

    return color;
  };

  /* Notice that the size is transposed, see the note on the horizontal pass method for more
   * information on the reasoning behind this. */
  const int2 size = int2(output.domain().size.y, output.domain().size.x);
  parallel_for(size, [&](const int2 texel) {
    float4 accumulated_color = float4(0.0f);

    /* First, compute the contribution of the center pixel. */
    float4 center_color = load_input(texel);
    accumulated_color += center_color * weights.load_pixel(int2(0)).x;

    /* Then, compute the contributions of the pixel to the right and left, noting that the
     * weights texture only stores the weights for the positive half, but since the filter is
     * symmetric, the same weight is used for the negative half and we add both of their
     * contributions. */
    for (int i = 1; i < weights.domain().size.x; i++) {
      float weight = weights.load_pixel(int2(i, 0)).x;
      accumulated_color += load_input(texel + int2(i, 0)) * weight;
      accumulated_color += load_input(texel + int2(-i, 0)) * weight;
    }

    if (gamma_uncorrect_output) {
      accumulated_color = gamma_uncorrect_blur_output(accumulated_color);
    }

    /* Write the color using the transposed texel. See the horizontal_pass method for more
     * information on the rational behind this. */
    output.store_pixel(int2(texel.y, texel.x), accumulated_color);
  });
}

static const char *get_blur_shader(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "compositor_symmetric_separable_blur_float";
    case ResultType::Float2:
      return "compositor_symmetric_separable_blur_float2";
    case ResultType::Vector:
    case ResultType::Color:
      return "compositor_symmetric_separable_blur_float4";
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

static Result horizontal_pass_gpu(Context &context,
                                  Result &input,
                                  float radius,
                                  int filter_type,
                                  bool extend_bounds,
                                  bool gamma_correct)
{
  GPUShader *shader = context.get_shader(get_blur_shader(input.type()));
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "extend_bounds", extend_bounds);
  GPU_shader_uniform_1b(shader, "gamma_correct_input", gamma_correct);
  GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", false);

  input.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius);
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
                                  Result &input,
                                  float radius,
                                  int filter_type,
                                  bool extend_bounds,
                                  bool gamma_correct)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius);

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

  Result output = context.create_result(input.type());
  output.allocate_texture(transposed_domain);

  blur_pass(input, weights, output, extend_bounds, gamma_correct, false);

  return output;
}

static Result horizontal_pass(Context &context,
                              Result &input,
                              float radius,
                              int filter_type,
                              bool extend_bounds,
                              bool gamma_correct)
{
  if (context.use_gpu()) {
    return horizontal_pass_gpu(context, input, radius, filter_type, extend_bounds, gamma_correct);
  }
  return horizontal_pass_cpu(context, input, radius, filter_type, extend_bounds, gamma_correct);
}

static void vertical_pass_gpu(Context &context,
                              Result &original_input,
                              Result &horizontal_pass_result,
                              Result &output,
                              float2 radius,
                              int filter_type,
                              bool extend_bounds,
                              bool gamma_correct)
{
  GPUShader *shader = context.get_shader(get_blur_shader(original_input.type()));
  GPU_shader_bind(shader);

  GPU_shader_uniform_1b(shader, "extend_bounds", extend_bounds);
  GPU_shader_uniform_1b(shader, "gamma_correct_input", false);
  GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", gamma_correct);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius.y);
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

static void vertical_pass_cpu(Context &context,
                              Result &original_input,
                              Result &horizontal_pass_result,
                              Result &output,
                              float2 radius,
                              int filter_type,
                              bool extend_bounds,
                              bool gamma_correct)
{
  const Result &weights = context.cache_manager().symmetric_separable_blur_weights.get(
      context, filter_type, radius.y);

  Domain domain = original_input.domain();
  if (extend_bounds) {
    /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
    domain.size += int2(math::ceil(radius)) * 2;
  }
  output.allocate_texture(domain);

  blur_pass(horizontal_pass_result, weights, output, extend_bounds, false, gamma_correct);
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
  if (context.use_gpu()) {
    vertical_pass_gpu(context,
                      original_input,
                      horizontal_pass_result,
                      output,
                      radius,
                      filter_type,
                      extend_bounds,
                      gamma_correct);
  }
  else {
    vertical_pass_cpu(context,
                      original_input,
                      horizontal_pass_result,
                      output,
                      radius,
                      filter_type,
                      extend_bounds,
                      gamma_correct);
  }
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
