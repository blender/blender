/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_morphological_distance.hh"

namespace blender::realtime_compositor {

static const char *get_shader_name(const int distance)
{
  if (distance > 0) {
    return "compositor_morphological_distance_dilate";
  }
  return "compositor_morphological_distance_erode";
}

void morphological_distance_gpu(Context &context,
                                const Result &input,
                                Result &output,
                                const int distance)
{
  GPUShader *shader = context.get_shader(get_shader_name(distance));
  GPU_shader_bind(shader);

  /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
  GPU_shader_uniform_1i(shader, "radius", math::abs(distance));

  input.bind_as_texture(shader, "input_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  output.unbind_as_image();
  input.unbind_as_texture();
}

void morphological_distance_cpu(const Result &input, Result &output, const int distance)
{
  output.allocate_texture(input.domain());

  /* We have specialized code for each sign, so use the absolute value. */
  const int radius = math::abs(distance);

  /* Find the minimum/maximum value in the circular window of the given radius around the pixel.
   * By circular window, we mean that pixels in the window whose distance to the center of window
   * is larger than the given radius are skipped and not considered. Consequently, the dilation
   * or erosion that take place produces round results as opposed to squarish ones. This is
   * essentially a morphological operator with a circular structuring element. */
  if (distance > 0) {
    parallel_for(input.domain().size, [&](const int2 texel) {
      float value = std::numeric_limits<float>::lowest();
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          if (x * x + y * y <= radius * radius) {
            const float4 fallback = float4(std::numeric_limits<float>::lowest());
            value = math::max(value, input.load_pixel_fallback(texel + int2(x, y), fallback).x);
          }
        }
      }

      output.store_pixel(texel, float4(value));
    });
  }
  else {
    parallel_for(input.domain().size, [&](const int2 texel) {
      float value = std::numeric_limits<float>::max();
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          if (x * x + y * y <= radius * radius) {
            const float4 fallback = float4(std::numeric_limits<float>::max());
            value = math::min(value, input.load_pixel_fallback(texel + int2(x, y), fallback).x);
          }
        }
      }

      output.store_pixel(texel, float4(value));
    });
  }
}

void morphological_distance(Context &context,
                            const Result &input,
                            Result &output,
                            const int distance)
{
  if (context.use_gpu()) {
    morphological_distance_gpu(context, input, output, distance);
  }
  else {
    morphological_distance_cpu(input, output, distance);
  }
}

}  // namespace blender::realtime_compositor
