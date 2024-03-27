/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "MEM_guardedalloc.h"

#include "COM_SunBeamsOperation.h"

namespace blender::compositor {

SunBeamsOperation::SunBeamsOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
}

void SunBeamsOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];

  const float2 input_size = float2(input->get_width(), input->get_height());
  const int max_steps = int(data_.ray_length * math::length(input_size));
  const float2 source = float2(data_.source);

  for (int y = area.ymin; y < area.ymax; y++) {
    for (int x = area.xmin; x < area.xmax; x++) {
      const float2 texel = float2(x, y);

      /* The number of steps is the distance in pixels from the source to the current texel. With
       * at least a single step and at most the user specified maximum ray length, which is
       * proportional to the diagonal pixel count. */
      const float unbounded_steps = math::max(1.0f, math::distance(texel, source * input_size));
      const int steps = math::min(max_steps, int(unbounded_steps));

      /* We integrate from the current pixel to the source pixel, so compute the start coordinates
       * and step vector in the direction to source. Notice that the step vector is still computed
       * from the unbounded steps, such that the total integration length becomes limited by the
       * bounded steps, and thus by the maximum ray length. */
      const float2 coordinates = (texel + float2(0.5f)) / input_size;
      const float2 vector_to_source = source - coordinates;
      const float2 step_vector = vector_to_source / unbounded_steps;

      float accumulated_weight = 0.0f;
      float4 accumulated_color = float4(0.0f);
      for (int i = 0; i <= steps; i++) {
        float2 position = coordinates + i * step_vector;

        /* We are already past the image boundaries, and any future steps are also past the image
         * boundaries, so break. */
        if (position.x < 0.0f || position.y < 0.0f || position.x > 1.0f || position.y > 1.0f) {
          break;
        }

        const float4 sample_color = input->texture_bilinear_extend(position);

        /* Attenuate the contributions of pixels that are further away from the source using a
         * quadratic falloff. Also weight by the alpha to give more significance to opaque pixels.
         */
        const float weight = (math::square(1.0f - i / float(steps))) * sample_color.w;

        accumulated_weight += weight;
        accumulated_color += sample_color * weight;
      }

      accumulated_color /= accumulated_weight != 0.0f ? accumulated_weight : 1.0f;
      copy_v4_v4(output->get_elem(x, y), accumulated_color);
    }
  }
}

}  // namespace blender::compositor
