/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_morphological_distance.hh"

namespace blender::compositor {

static const char *get_shader_name(const int distance)
{
  if (distance > 0) {
    return "compositor_morphological_distance_dilate";
  }
  return "compositor_morphological_distance_erode";
}

static void morphological_distance_gpu(Context &context,
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

template<bool IsDilate>
static void morphological_distance_cpu(const Result &input,
                                       Result &output,
                                       const int structuring_element_radius)
{
  output.allocate_texture(input.domain());

  const float limit = IsDilate ? std::numeric_limits<float>::lowest() :
                                 std::numeric_limits<float>::max();
  const auto morphology_operator = [](const float a, const float b) {
    if constexpr (IsDilate) {
      return math::max(a, b);
    }
    else {
      return math::min(a, b);
    }
  };

  const int2 image_size = input.domain().size;

  const int radius_squared = math::square(structuring_element_radius);

  /* Find the minimum/maximum value in the circular window of the given radius around the pixel.
   * By circular window, we mean that pixels in the window whose distance to the center of window
   * is larger than the given radius are skipped and not considered. Consequently, the dilation
   * or erosion that take place produces round results as opposed to squarish ones. This is
   * essentially a morphological operator with a circular structuring element. */
  parallel_for(image_size, [&](const int2 texel) {
    /* Compute the start and end bounds of the window such that no out-of-bounds processing happen
     * in the loops. */
    const int2 start = math::max(texel - structuring_element_radius, int2(0)) - texel;
    const int2 end = math::min(texel + structuring_element_radius + 1, image_size) - texel;

    float value = limit;
    for (int y = start.y; y < end.y; y++) {
      const int yy = y * y;
      for (int x = start.x; x < end.x; x++) {
        if (x * x + yy > radius_squared) {
          continue;
        }
        value = morphology_operator(value, input.load_pixel<float>(texel + int2(x, y)));
      }
    }

    output.store_pixel(texel, value);
  });
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
    if (distance > 0) {
      morphological_distance_cpu<true>(input, output, math::abs(distance));
    }
    else {
      morphological_distance_cpu<false>(input, output, math::abs(distance));
    }
  }
}

}  // namespace blender::compositor
