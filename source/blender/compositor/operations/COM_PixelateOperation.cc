/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PixelateOperation.h"

#include <algorithm>

namespace blender::compositor {

PixelateOperation::PixelateOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);

  flags_.can_be_constant = true;

  pixel_size_ = 1;
}

void PixelateOperation::get_area_of_interest(const int /*input_idx*/,
                                             const rcti &output_area,
                                             rcti &r_input_area)
{
  r_input_area.xmin = output_area.xmin;
  r_input_area.ymin = output_area.ymin;

  r_input_area.xmax = output_area.xmax + pixel_size_ - 1;
  r_input_area.ymax = output_area.ymax + pixel_size_ - 1;
}

void PixelateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image = inputs[0];

  if (image->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), image->get_elem(0, 0));
    return;
  }

  const int width = image->get_width();
  const int height = image->get_height();

  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const int x_start = (it.x / pixel_size_) * pixel_size_;
    const int y_start = (it.y / pixel_size_) * pixel_size_;

    const int x_end = std::min(x_start + pixel_size_, width);
    const int y_end = std::min(y_start + pixel_size_, height);

    float4 color_accum(0, 0, 0, 0);

    for (int y = y_start; y < y_end; ++y) {
      for (int x = x_start; x < x_end; ++x) {
        float4 color;
        image->read_elem(x, y, color);

        color_accum += color;
      }
    }

    const int scale = (x_end - x_start) * (y_end - y_start);

    copy_v4_v4(it.out, color_accum / float(scale));
  }
}

}  // namespace blender::compositor
