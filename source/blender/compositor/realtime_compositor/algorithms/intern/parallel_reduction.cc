/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "MEM_guardedalloc.h"

#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"

#include "COM_algorithm_parallel_reduction.hh"

namespace blender::realtime_compositor {

/* Reduces the given texture into a single value and returns it. The return value should be freed
 * by a call to MEM_freeN. The return value is either a pointer to a float, or a pointer to an
 * array of floats that represents a vector. This depends on the given format, which should be
 * compatible with the reduction shader.
 *
 * The given reduction shader should be bound when calling the function and the shader is expected
 * to be derived from the compositor_parallel_reduction.glsl shader, see that file for more
 * information. Also see the compositor_parallel_reduction_info.hh file for example shader
 * definitions. */
static float *parallel_reduction_dispatch(Context &context,
                                          GPUTexture *texture,
                                          GPUShader *shader,
                                          eGPUTextureFormat format)
{
  GPU_shader_uniform_1b(shader, "is_initial_reduction", true);

  GPUTexture *texture_to_reduce = texture;
  int2 size_to_reduce = int2(GPU_texture_width(texture), GPU_texture_height(texture));

  /* Dispatch the reduction shader until the texture reduces to a single pixel. */
  while (size_to_reduce != int2(1)) {
    const int2 reduced_size = math::divide_ceil(size_to_reduce, int2(16));
    GPUTexture *reduced_texture = context.texture_pool().acquire(reduced_size, format);

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    const int texture_image_unit = GPU_shader_get_sampler_binding(shader, "input_tx");
    GPU_texture_bind(texture_to_reduce, texture_image_unit);

    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(reduced_texture, image_unit);

    GPU_compute_dispatch(shader, reduced_size.x, reduced_size.y, 1);

    GPU_texture_image_unbind(reduced_texture);
    GPU_texture_unbind(texture_to_reduce);

    /* Release the input texture only if it is not the source texture, since the source texture is
     * not acquired or owned by the function. */
    if (texture_to_reduce != texture) {
      context.texture_pool().release(texture_to_reduce);
    }

    texture_to_reduce = reduced_texture;
    size_to_reduce = reduced_size;

    GPU_shader_uniform_1b(shader, "is_initial_reduction", false);
  }

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float *pixel = static_cast<float *>(GPU_texture_read(texture_to_reduce, GPU_DATA_FLOAT, 0));

  /* Release the final texture only if it is not the source texture, since the source texture is
   * not acquired or owned by the function. */
  if (texture_to_reduce != texture) {
    context.texture_pool().release(texture_to_reduce);
  }

  return pixel;
}

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

float sum_red(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_sum_red", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_green(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_sum_green", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_blue(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_sum_blue", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_luminance(Context &context, GPUTexture *texture, float3 luminance_coefficients)
{
  GPUShader *shader = context.get_shader("compositor_sum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_log_luminance(Context &context, GPUTexture *texture, float3 luminance_coefficients)
{
  GPUShader *shader = context.get_shader("compositor_sum_log_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float4 sum_color(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_sum_color", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Color, ResultPrecision::Full));
  const float4 sum = float4(reduced_value);
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

float sum_red_squared_difference(Context &context, GPUTexture *texture, float subtrahend)
{
  GPUShader *shader = context.get_shader("compositor_sum_red_squared_difference",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_green_squared_difference(Context &context, GPUTexture *texture, float subtrahend)
{
  GPUShader *shader = context.get_shader("compositor_sum_green_squared_difference",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_blue_squared_difference(Context &context, GPUTexture *texture, float subtrahend)
{
  GPUShader *shader = context.get_shader("compositor_sum_blue_squared_difference",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

float sum_luminance_squared_difference(Context &context,
                                       GPUTexture *texture,
                                       float3 luminance_coefficients,
                                       float subtrahend)
{
  GPUShader *shader = context.get_shader("compositor_sum_luminance_squared_difference",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

float maximum_luminance(Context &context, GPUTexture *texture, float3 luminance_coefficients)
{
  GPUShader *shader = context.get_shader("compositor_maximum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

float maximum_float(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_maximum_float", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

float maximum_float_in_range(Context &context,
                             GPUTexture *texture,
                             float lower_bound,
                             float upper_bound)
{
  GPUShader *shader = context.get_shader("compositor_maximum_float_in_range",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "lower_bound", lower_bound);
  GPU_shader_uniform_1f(shader, "upper_bound", upper_bound);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

float minimum_luminance(Context &context, GPUTexture *texture, float3 luminance_coefficients)
{
  GPUShader *shader = context.get_shader("compositor_minimum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

float minimum_float(Context &context, GPUTexture *texture)
{
  GPUShader *shader = context.get_shader("compositor_minimum_float", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

float minimum_float_in_range(Context &context,
                             GPUTexture *texture,
                             float lower_bound,
                             float upper_bound)
{
  GPUShader *shader = context.get_shader("compositor_minimum_float_in_range",
                                         ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "lower_bound", lower_bound);
  GPU_shader_uniform_1f(shader, "upper_bound", upper_bound);

  float *reduced_value = parallel_reduction_dispatch(
      context, texture, shader, Result::texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

}  // namespace blender::realtime_compositor
