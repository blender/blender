/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <limits>

#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "MEM_guardedalloc.h"

#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_texture_pool.hh"

#include "COM_context.hh"
#include "COM_result.hh"

#include "COM_algorithm_parallel_reduction.hh"

namespace blender::compositor {

/* Reduces the given texture into a single value and returns it. The return value should be freed
 * by a call to MEM_freeN. The return value is either a pointer to a float, or a pointer to an
 * array of floats that represents a vector. This depends on the given format, which should be
 * compatible with the reduction shader.
 *
 * The given reduction shader should be bound when calling the function and the shader is expected
 * to be derived from the compositor_parallel_reduction.glsl shader, see that file for more
 * information. Also see the compositor_parallel_reduction_info.hh file for example shader
 * definitions. */
static float *parallel_reduction_dispatch(blender::gpu::Texture *texture,
                                          gpu::Shader *shader,
                                          blender::gpu::TextureFormat format)
{
  GPU_shader_uniform_1b(shader, "is_initial_reduction", true);

  blender::gpu::Texture *texture_to_reduce = texture;
  int2 size_to_reduce = int2(GPU_texture_width(texture), GPU_texture_height(texture));

  /* Dispatch the reduction shader until the texture reduces to a single pixel. */
  while (size_to_reduce != int2(1)) {
    const int2 reduced_size = math::divide_ceil(size_to_reduce, int2(16));
    blender::gpu::Texture *reduced_texture = gpu::TexturePool::get().acquire_texture(
        reduced_size.x, reduced_size.y, format, GPU_TEXTURE_USAGE_GENERAL);

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
      gpu::TexturePool::get().release_texture(texture_to_reduce);
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
    gpu::TexturePool::get().release_texture(texture_to_reduce);
  }

  return pixel;
}

/* Reduces the given function in parallel over the given 2D range, the reduction function should
 * have the given identity value. The given function gets as arguments the texel coordinates of the
 * element of the range as well as a reference to the value where the result should be accumulated,
 * while the reduction function gets a reference to two values and returns their reduction. */
template<typename Value, typename Function, typename Reduction>
static Value parallel_reduce(const int2 range,
                             const Value &identity,
                             const Function &function,
                             const Reduction &reduction)
{
  return threading::parallel_reduce(
      IndexRange(range.y),
      64,
      identity,
      [&](const IndexRange sub_y_range, const Value &initial_value) {
        Value result = initial_value;
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : IndexRange(range.x)) {
            function(int2(x, y), result);
          }
        }
        return result;
      },
      reduction);
}

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

static float sum_red_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_red", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_red_cpu(const Result &result)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += result.load_pixel<Color>(texel).r;
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_red(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return sum_red_gpu(context, result);
  }

  return sum_red_cpu(result);
}

static float sum_green_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_green", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_green_cpu(const Result &result)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += result.load_pixel<Color>(texel).g;
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_green(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return sum_green_gpu(context, result);
  }

  return sum_green_cpu(result);
}

static float sum_blue_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_blue", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_blue_cpu(const Result &result)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += result.load_pixel<Color>(texel).b;
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_blue(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return sum_blue_gpu(context, result);
  }

  return sum_blue_cpu(result);
}

static float sum_luminance_gpu(Context &context,
                               const Result &result,
                               const float3 &luminance_coefficients)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_luminance_cpu(const Result &result, const float3 &luminance_coefficients)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += math::dot(float4(result.load_pixel<Color>(texel)).xyz(),
                                       luminance_coefficients);
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_luminance(Context &context, const Result &result, const float3 &luminance_coefficients)
{
  if (context.use_gpu()) {
    return sum_luminance_gpu(context, result, luminance_coefficients);
  }

  return sum_luminance_cpu(result, luminance_coefficients);
}

static float sum_log_luminance_gpu(Context &context,
                                   const Result &result,
                                   const float3 &luminance_coefficients)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_log_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_log_luminance_cpu(const Result &result, const float3 &luminance_coefficients)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        const float luminance = math::dot(float4(result.load_pixel<Color>(texel)).xyz(),
                                          luminance_coefficients);
        accumulated_value += std::log(math::max(luminance, 1e-5f));
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_log_luminance(Context &context,
                        const Result &result,
                        const float3 &luminance_coefficients)
{
  if (context.use_gpu()) {
    return sum_log_luminance_gpu(context, result, luminance_coefficients);
  }

  return sum_log_luminance_cpu(result, luminance_coefficients);
}

static float4 sum_color_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_color", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Color, ResultPrecision::Full));
  const float4 sum = float4(reduced_value);
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float4 sum_color_cpu(const Result &result)
{
  return float4(parallel_reduce(
      result.domain().size,
      double4(0.0),
      [&](const int2 texel, double4 &accumulated_value) {
        accumulated_value += double4(float4(result.load_pixel<Color>(texel)));
      },
      [&](const double4 &a, const double4 &b) { return a + b; }));
}

float4 sum_color(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return sum_color_gpu(context, result);
  }

  return sum_color_cpu(result);
}

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

static float sum_red_squared_difference_gpu(Context &context,
                                            const Result &result,
                                            const float subtrahend)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_red_squared_difference",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_red_squared_difference_cpu(const Result &result, const float subtrahend)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += math::square(result.load_pixel<Color>(texel).r - subtrahend);
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_red_squared_difference(Context &context, const Result &result, const float subtrahend)
{
  if (context.use_gpu()) {
    return sum_red_squared_difference_gpu(context, result, subtrahend);
  }

  return sum_red_squared_difference_cpu(result, subtrahend);
}

static float sum_green_squared_difference_gpu(Context &context,
                                              const Result &result,
                                              const float subtrahend)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_green_squared_difference",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_green_squared_difference_cpu(const Result &result, const float subtrahend)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += math::square(result.load_pixel<Color>(texel).g - subtrahend);
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_green_squared_difference(Context &context, const Result &result, const float subtrahend)
{
  if (context.use_gpu()) {
    return sum_green_squared_difference_gpu(context, result, subtrahend);
  }

  return sum_green_squared_difference_cpu(result, subtrahend);
}

static float sum_blue_squared_difference_gpu(Context &context,
                                             const Result &result,
                                             const float subtrahend)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_blue_squared_difference",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_blue_squared_difference_cpu(const Result &result, const float subtrahend)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        accumulated_value += math::square(result.load_pixel<Color>(texel).b - subtrahend);
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_blue_squared_difference(Context &context, const Result &result, const float subtrahend)
{
  if (context.use_gpu()) {
    return sum_blue_squared_difference_gpu(context, result, subtrahend);
  }

  return sum_blue_squared_difference_cpu(result, subtrahend);
}

static float sum_luminance_squared_difference_gpu(Context &context,
                                                  const Result &result,
                                                  const float3 &luminance_coefficients,
                                                  const float subtrahend)
{
  gpu::Shader *shader = context.get_shader("compositor_sum_luminance_squared_difference",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
  GPU_shader_uniform_1f(shader, "subtrahend", subtrahend);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float sum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return sum;
}

static float sum_luminance_squared_difference_cpu(const Result &result,
                                                  const float3 &luminance_coefficients,
                                                  const float subtrahend)
{
  return float(parallel_reduce(
      result.domain().size,
      0.0,
      [&](const int2 texel, double &accumulated_value) {
        const float luminance = math::dot(float4(result.load_pixel<Color>(texel)).xyz(),
                                          luminance_coefficients);
        accumulated_value += math::square(luminance - subtrahend);
      },
      [&](const double &a, const double &b) { return a + b; }));
}

float sum_luminance_squared_difference(Context &context,
                                       const Result &result,
                                       const float3 &luminance_coefficients,
                                       const float subtrahend)
{
  if (context.use_gpu()) {
    return sum_luminance_squared_difference_gpu(
        context, result, luminance_coefficients, subtrahend);
  }

  return sum_luminance_squared_difference_cpu(result, luminance_coefficients, subtrahend);
}

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

static float maximum_luminance_gpu(Context &context,
                                   const Result &result,
                                   const float3 &luminance_coefficients)
{
  gpu::Shader *shader = context.get_shader("compositor_maximum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

static float maximum_luminance_cpu(const Result &result, const float3 &luminance_coefficients)
{
  return float(parallel_reduce(
      result.domain().size,
      std::numeric_limits<float>::lowest(),
      [&](const int2 texel, float &accumulated_value) {
        const float luminance = math::dot(float4(result.load_pixel<Color>(texel)).xyz(),
                                          luminance_coefficients);
        accumulated_value = math::max(accumulated_value, luminance);
      },
      [&](const float &a, const float &b) { return math::max(a, b); }));
}

float maximum_luminance(Context &context,
                        const Result &result,
                        const float3 &luminance_coefficients)
{
  if (context.use_gpu()) {
    return maximum_luminance_gpu(context, result, luminance_coefficients);
  }

  return maximum_luminance_cpu(result, luminance_coefficients);
}

static float maximum_float_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_maximum_float", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

static float maximum_float_cpu(const Result &result)
{
  return float(parallel_reduce(
      result.domain().size,
      std::numeric_limits<float>::lowest(),
      [&](const int2 texel, float &accumulated_value) {
        accumulated_value = math::max(accumulated_value, result.load_pixel<float>(texel));
      },
      [&](const float &a, const float &b) { return math::max(a, b); }));
}

float maximum_float(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return maximum_float_gpu(context, result);
  }

  return maximum_float_cpu(result);
}

static float2 maximum_float2_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_maximum_float2", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float2, ResultPrecision::Full));
  const float2 maximum = reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

static float2 maximum_float2_cpu(const Result &result)
{
  return parallel_reduce(
      result.domain().size,
      float2(std::numeric_limits<float>::lowest()),
      [&](const int2 texel, float2 &accumulated_value) {
        accumulated_value = math::max(accumulated_value, result.load_pixel<float2>(texel));
      },
      [&](const float2 &a, const float2 &b) { return math::max(a, b); });
}

float2 maximum_float2(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return maximum_float2_gpu(context, result);
  }

  return maximum_float2_cpu(result);
}

static float maximum_float_in_range_gpu(Context &context,
                                        const Result &result,
                                        const float lower_bound,
                                        const float upper_bound)
{
  gpu::Shader *shader = context.get_shader("compositor_maximum_float_in_range",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "lower_bound", lower_bound);
  GPU_shader_uniform_1f(shader, "upper_bound", upper_bound);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float maximum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return maximum;
}

static float maximum_float_in_range_cpu(const Result &result,
                                        const float lower_bound,
                                        const float upper_bound)
{
  return float(parallel_reduce(
      result.domain().size,
      lower_bound,
      [&](const int2 texel, float &accumulated_value) {
        const float value = result.load_pixel<float>(texel);
        if ((value <= upper_bound) && (value >= lower_bound)) {
          accumulated_value = math::max(accumulated_value, value);
        }
      },
      [&](const float &a, const float &b) { return math::max(a, b); }));
}

float maximum_float_in_range(Context &context,
                             const Result &result,
                             const float lower_bound,
                             const float upper_bound)
{
  if (context.use_gpu()) {
    return maximum_float_in_range_gpu(context, result, lower_bound, upper_bound);
  }

  return maximum_float_in_range_cpu(result, lower_bound, upper_bound);
}

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

static float minimum_luminance_gpu(Context &context,
                                   const Result &result,
                                   const float3 &luminance_coefficients)
{
  gpu::Shader *shader = context.get_shader("compositor_minimum_luminance", ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

static float minimum_luminance_cpu(const Result &result, const float3 &luminance_coefficients)
{
  return float(parallel_reduce(
      result.domain().size,
      std::numeric_limits<float>::max(),
      [&](const int2 texel, float &accumulated_value) {
        const float luminance = math::dot(float4(result.load_pixel<Color>(texel)).xyz(),
                                          luminance_coefficients);
        accumulated_value = math::min(accumulated_value, luminance);
      },
      [&](const float &a, const float &b) { return math::min(a, b); }));
}

float minimum_luminance(Context &context,
                        const Result &result,
                        const float3 &luminance_coefficients)
{
  if (context.use_gpu()) {
    return minimum_luminance_gpu(context, result, luminance_coefficients);
  }

  return minimum_luminance_cpu(result, luminance_coefficients);
}

static float minimum_float_gpu(Context &context, const Result &result)
{
  gpu::Shader *shader = context.get_shader("compositor_minimum_float", ResultPrecision::Full);
  GPU_shader_bind(shader);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

static float minimum_float_cpu(const Result &result)
{
  return float(parallel_reduce(
      result.domain().size,
      std::numeric_limits<float>::max(),
      [&](const int2 texel, float &accumulated_value) {
        accumulated_value = math::min(accumulated_value, result.load_pixel<float>(texel));
      },
      [&](const float &a, const float &b) { return math::min(a, b); }));
}

float minimum_float(Context &context, const Result &result)
{
  if (context.use_gpu()) {
    return minimum_float_gpu(context, result);
  }

  return minimum_float_cpu(result);
}

static float minimum_float_in_range_gpu(Context &context,
                                        const Result &result,
                                        const float lower_bound,
                                        const float upper_bound)
{
  gpu::Shader *shader = context.get_shader("compositor_minimum_float_in_range",
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "lower_bound", lower_bound);
  GPU_shader_uniform_1f(shader, "upper_bound", upper_bound);

  float *reduced_value = parallel_reduction_dispatch(
      result, shader, Result::gpu_texture_format(ResultType::Float, ResultPrecision::Full));
  const float minimum = *reduced_value;
  MEM_freeN(reduced_value);
  GPU_shader_unbind();

  return minimum;
}

static float minimum_float_in_range_cpu(const Result &result,
                                        const float lower_bound,
                                        const float upper_bound)
{
  return parallel_reduce(
      result.domain().size,
      upper_bound,
      [&](const int2 texel, float &accumulated_value) {
        const float value = result.load_pixel<float>(texel);
        if ((value <= upper_bound) && (value >= lower_bound)) {
          accumulated_value = math::min(accumulated_value, value);
        }
      },
      [&](const float &a, const float &b) { return math::min(a, b); });
}

float minimum_float_in_range(Context &context,
                             const Result &result,
                             const float lower_bound,
                             const float upper_bound)
{
  if (context.use_gpu()) {
    return minimum_float_in_range_gpu(context, result, lower_bound, upper_bound);
  }

  return minimum_float_in_range_cpu(result, lower_bound, upper_bound);
}

}  // namespace blender::compositor
