/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KeyingClipOperation.h"

namespace blender::compositor {

KeyingClipOperation::KeyingClipOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);

  kernel_radius_ = 3;
  kernel_tolerance_ = 0.1f;

  clip_black_ = 0.0f;
  clip_white_ = 1.0f;

  is_edge_matte_ = false;

  flags_.can_be_constant = true;
}

void KeyingClipOperation::get_area_of_interest(const int input_idx,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - kernel_radius_;
  r_input_area.xmax = output_area.xmax + kernel_radius_;
  r_input_area.ymin = output_area.ymin - kernel_radius_;
  r_input_area.ymax = output_area.ymax + kernel_radius_;
}

void KeyingClipOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  BuffersIterator<float> it = output->iterate_with(inputs, area);

  const int delta = kernel_radius_;
  const float tolerance = kernel_tolerance_;
  const int width = this->get_width();
  const int height = this->get_height();
  const int row_stride = input->row_stride;
  const int elem_stride = input->elem_stride;
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;

    const int start_x = std::max(0, x - delta);
    const int start_y = std::max(0, y - delta);
    const int end_x = std::min(x + delta, width - 1);
    const int end_y = std::min(y + delta, height - 1);
    const int x_len = end_x - start_x + 1;
    const int y_len = end_y - start_y + 1;

    const int total_count = x_len * y_len;
    const int threshold_count = ceil(float(total_count) * 0.9f);
    bool ok = false;
    if (delta == 0) {
      ok = true;
    }

    const float *main_elem = it.in(0);
    const float value = *main_elem;
    const float *row = input->get_elem(start_x, start_y);
    const float *end_row = row + y_len * row_stride;
    int count = 0;
    for (; ok == false && row < end_row; row += row_stride) {
      const float *end_elem = row + x_len * elem_stride;
      for (const float *elem = row; ok == false && elem < end_elem; elem += elem_stride) {
        const float current_value = *elem;
        if (fabsf(current_value - value) < tolerance) {
          count++;
          if (count >= threshold_count) {
            ok = true;
          }
        }
      }
    }

    if (is_edge_matte_) {
      *it.out = ok ? 0.0f : 1.0f;
    }
    else {
      if (!ok) {
        *it.out = value;
      }
      else if (value < clip_black_) {
        *it.out = 0.0f;
      }
      else if (value >= clip_white_) {
        *it.out = 1.0f;
      }
      else {
        *it.out = (value - clip_black_) / (clip_white_ - clip_black_);
      }
    }
  }
}

}  // namespace blender::compositor
