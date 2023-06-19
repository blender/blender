/* SPDX-FileCopyrightText: 2012 Blender Foundation
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

  flags_.complex = true;
}

void *KeyingClipOperation::initialize_tile_data(rcti *rect)
{
  void *buffer = get_input_operation(0)->initialize_tile_data(rect);

  return buffer;
}

void KeyingClipOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const int delta = kernel_radius_;
  const float tolerance = kernel_tolerance_;

  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();

  int buffer_width = input_buffer->get_width();
  int buffer_height = input_buffer->get_height();

  float value = buffer[(y * buffer_width + x)];

  bool ok = false;
  int start_x = max_ff(0, x - delta + 1), start_y = max_ff(0, y - delta + 1),
      end_x = min_ff(x + delta - 1, buffer_width - 1),
      end_y = min_ff(y + delta - 1, buffer_height - 1);

  int count = 0, total_count = (end_x - start_x + 1) * (end_y - start_y + 1) - 1;
  int threshold_count = ceil(float(total_count) * 0.9f);

  if (delta == 0) {
    ok = true;
  }

  for (int cx = start_x; ok == false && cx <= end_x; cx++) {
    for (int cy = start_y; ok == false && cy <= end_y; cy++) {
      if (UNLIKELY(cx == x && cy == y)) {
        continue;
      }

      int buffer_index = (cy * buffer_width + cx);
      float current_value = buffer[buffer_index];

      if (fabsf(current_value - value) < tolerance) {
        count++;
        if (count >= threshold_count) {
          ok = true;
        }
      }
    }
  }

  if (is_edge_matte_) {
    if (ok) {
      output[0] = 0.0f;
    }
    else {
      output[0] = 1.0f;
    }
  }
  else {
    output[0] = value;

    if (ok) {
      if (output[0] < clip_black_) {
        output[0] = 0.0f;
      }
      else if (output[0] >= clip_white_) {
        output[0] = 1.0f;
      }
      else {
        output[0] = (output[0] - clip_black_) / (clip_white_ - clip_black_);
      }
    }
  }
}

bool KeyingClipOperation::determine_depending_area_of_interest(rcti *input,
                                                               ReadBufferOperation *read_operation,
                                                               rcti *output)
{
  rcti new_input;

  new_input.xmin = input->xmin - kernel_radius_;
  new_input.ymin = input->ymin - kernel_radius_;
  new_input.xmax = input->xmax + kernel_radius_;
  new_input.ymax = input->ymax + kernel_radius_;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
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

    const int start_x = MAX2(0, x - delta + 1);
    const int start_y = MAX2(0, y - delta + 1);
    const int end_x = MIN2(x + delta, width);
    const int end_y = MIN2(y + delta, height);
    const int x_len = end_x - start_x;
    const int y_len = end_y - start_y;

    const int total_count = x_len * y_len - 1;
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
        if (UNLIKELY(elem == main_elem)) {
          continue;
        }

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
