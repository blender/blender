/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "COM_KuwaharaClassicOperation.h"
#include "COM_SummedAreaTableOperation.h"

namespace blender::compositor {

KuwaharaClassicOperation::KuwaharaClassicOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  this->flags_.can_be_constant = true;
}

void KuwaharaClassicOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image = inputs[0];
  if (image->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), image->get_elem(0, 0));
    return;
  }
  MemoryBuffer *size_image = inputs[1];
  MemoryBuffer *sat = inputs[2];
  MemoryBuffer *sat_squared = inputs[3];

  int width = image->get_width();
  int height = image->get_height();

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    float4 mean_of_color[4] = {float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f)};
    float4 mean_of_squared_color[4] = {float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f)};
    int quadrant_pixel_count[4] = {0, 0, 0, 0};

    const float size = *size_image->get_elem(x, y);
    const int kernel_size = int(math::max(0.0f, size));

    /* For high radii, we accelerate the filter using a summed area table, making the filter
     * execute in constant time as opposed to having quadratic complexity. Except if high precision
     * is enabled, since summed area tables are less precise. */
    if (!high_precision_ && size > 5.0f) {
      for (int q = 0; q < 4; q++) {
        /* A fancy expression to compute the sign of the quadrant q. */
        int2 sign = int2((q % 2) * 2 - 1, ((q / 2) * 2 - 1));

        int2 lower_bound = int2(x, y) -
                           int2(sign.x > 0 ? 0 : kernel_size, sign.y > 0 ? 0 : kernel_size);
        int2 upper_bound = int2(x, y) +
                           int2(sign.x < 0 ? 0 : kernel_size, sign.y < 0 ? 0 : kernel_size);

        /* Limit the quadrants to the image bounds. */
        int2 image_bound = int2(width, height) - int2(1);
        int2 corrected_lower_bound = math::min(image_bound, math::max(int2(0, 0), lower_bound));
        int2 corrected_upper_bound = math::min(image_bound, math::max(int2(0, 0), upper_bound));
        int2 region_size = corrected_upper_bound - corrected_lower_bound + int2(1, 1);
        quadrant_pixel_count[q] = region_size.x * region_size.y;

        rcti kernel_area;
        kernel_area.xmin = corrected_lower_bound[0];
        kernel_area.ymin = corrected_lower_bound[1];
        kernel_area.xmax = corrected_upper_bound[0];
        kernel_area.ymax = corrected_upper_bound[1];

        mean_of_color[q] = summed_area_table_sum(sat, kernel_area);
        mean_of_squared_color[q] = summed_area_table_sum(sat_squared, kernel_area);
      }
    }
    else {
      /* Split surroundings of pixel into 4 overlapping regions. */
      for (int dy = -kernel_size; dy <= kernel_size; dy++) {
        for (int dx = -kernel_size; dx <= kernel_size; dx++) {

          int xx = x + dx;
          int yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= image->get_width() || yy >= image->get_height()) {
            continue;
          }

          float4 color;
          image->read_elem(xx, yy, &color.x);

          if (dx >= 0 && dy >= 0) {
            const int quadrant_index = 0;
            mean_of_color[quadrant_index] += color;
            mean_of_squared_color[quadrant_index] += color * color;
            quadrant_pixel_count[quadrant_index]++;
          }

          if (dx <= 0 && dy >= 0) {
            const int quadrant_index = 1;
            mean_of_color[quadrant_index] += color;
            mean_of_squared_color[quadrant_index] += color * color;
            quadrant_pixel_count[quadrant_index]++;
          }

          if (dx <= 0 && dy <= 0) {
            const int quadrant_index = 2;
            mean_of_color[quadrant_index] += color;
            mean_of_squared_color[quadrant_index] += color * color;
            quadrant_pixel_count[quadrant_index]++;
          }

          if (dx >= 0 && dy <= 0) {
            const int quadrant_index = 3;
            mean_of_color[quadrant_index] += color;
            mean_of_squared_color[quadrant_index] += color * color;
            quadrant_pixel_count[quadrant_index]++;
          }
        }
      }
    }

    /* Choose the region with lowest variance. */
    float min_var = FLT_MAX;
    int min_index = 0;
    for (int i = 0; i < 4; i++) {
      mean_of_color[i] /= quadrant_pixel_count[i];
      mean_of_squared_color[i] /= quadrant_pixel_count[i];
      float4 color_variance = mean_of_squared_color[i] - mean_of_color[i] * mean_of_color[i];

      float variance = math::dot(color_variance.xyz(), float3(1.0f));
      if (variance < min_var) {
        min_var = variance;
        min_index = i;
      }
    }

    it.out[0] = mean_of_color[min_index].x;
    it.out[1] = mean_of_color[min_index].y;
    it.out[2] = mean_of_color[min_index].z;
    it.out[3] = mean_of_color[min_index].w; /* Also apply filter to alpha channel. */
  }
}

}  // namespace blender::compositor
