/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_task.hh"

#include "DNA_scene_types.h"

#include "RE_pipeline.h"

#include "COM_MemoryBuffer.h"
#include "COM_SymmetricSeparableBlurVariableSizeAlgorithm.h"

namespace blender::compositor {

static MemoryBuffer compute_symmetric_separable_blur_weights(int type, float radius)
{
  const int size = math::ceil(radius) + 1;
  rcti rect;
  BLI_rcti_init(&rect, 0, size, 0, 1);
  MemoryBuffer weights = MemoryBuffer(DataType::Value, rect);

  float sum = 0.0f;

  const float center_weight = RE_filter_value(type, 0.0f);
  *weights.get_elem(0, 0) = center_weight;
  sum += center_weight;

  const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
  for (const int i : IndexRange(size).drop_front(1)) {
    const float weight = RE_filter_value(type, i * scale);
    *weights.get_elem(i, 0) = weight;
    sum += weight * 2.0f;
  }

  for (const int i : IndexRange(size)) {
    *weights.get_elem(i, 0) /= sum;
  }

  return weights;
}

static float sample_weight(const MemoryBuffer &weights, float parameter)
{
  const int size = weights.get_width();
  float weight;
  weights.read_elem_bilinear(parameter * size, 0.0f, &weight);
  return weight;
}

static void blur_pass(const MemoryBuffer &input,
                      const MemoryBuffer &radius_buffer,
                      const MemoryBuffer &weights,
                      MemoryBuffer &output,
                      bool is_vertical)
{
  const int2 size = int2(input.get_width(), input.get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        float accumulated_weight = 0.0f;
        float4 accumulated_color = float4(0.0f);

        float4 center_color = float4(input.get_elem(x, y));
        float center_weight = *weights.get_elem(0, 0);
        accumulated_color += center_color * center_weight;
        accumulated_weight += center_weight;

        int radius = int(
            *(is_vertical ? radius_buffer.get_elem(y, x) : radius_buffer.get_elem(x, y)));

        for (int i = 1; i <= radius; i++) {
          float weight = sample_weight(weights, (float(i) + 0.5f) / float(radius + 1));
          accumulated_color += float4(input.get_elem_clamped(x + i, y)) * weight;
          accumulated_color += float4(input.get_elem_clamped(x - i, y)) * weight;
          accumulated_weight += weight * 2.0f;
        }

        const float4 final_color = accumulated_color / accumulated_weight;
        copy_v4_v4(output.get_elem(y, x), final_color);
      }
    }
  });
}

void symmetric_separable_blur_variable_size(const MemoryBuffer &input,
                                            MemoryBuffer &output,
                                            const MemoryBuffer &radius,
                                            int filter_type,
                                            int weights_resolution)
{
  const MemoryBuffer weights = compute_symmetric_separable_blur_weights(filter_type,
                                                                        weights_resolution);
  rcti rect;
  BLI_rcti_init(&rect, 0, input.get_height(), 0, input.get_width());
  MemoryBuffer horizontal_pass_result = MemoryBuffer(DataType::Color, rect);

  blur_pass(input, radius, weights, horizontal_pass_result, false);
  blur_pass(horizontal_pass_result, radius, weights, output, true);
}

}  // namespace blender::compositor
