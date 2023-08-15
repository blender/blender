/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KeyingBlurOperation.h"

namespace blender::compositor {

KeyingBlurOperation::KeyingBlurOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);

  size_ = 0;
  axis_ = BLUR_AXIS_X;

  flags_.complex = true;
}

void *KeyingBlurOperation::initialize_tile_data(rcti *rect)
{
  void *buffer = get_input_operation(0)->initialize_tile_data(rect);

  return buffer;
}

void KeyingBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const int buffer_width = input_buffer->get_width();
  float *buffer = input_buffer->get_buffer();
  int count = 0;
  float average = 0.0f;

  if (axis_ == 0) {
    const int start = MAX2(0, x - size_ + 1), end = MIN2(buffer_width, x + size_);
    for (int cx = start; cx < end; cx++) {
      int buffer_index = (y * buffer_width + cx);
      average += buffer[buffer_index];
      count++;
    }
  }
  else {
    const int start = MAX2(0, y - size_ + 1), end = MIN2(input_buffer->get_height(), y + size_);
    for (int cy = start; cy < end; cy++) {
      int buffer_index = (cy * buffer_width + x);
      average += buffer[buffer_index];
      count++;
    }
  }

  average /= float(count);

  output[0] = average;
}

bool KeyingBlurOperation::determine_depending_area_of_interest(rcti *input,
                                                               ReadBufferOperation *read_operation,
                                                               rcti *output)
{
  rcti new_input;

  if (axis_ == BLUR_AXIS_X) {
    new_input.xmin = input->xmin - size_;
    new_input.ymin = input->ymin;
    new_input.xmax = input->xmax + size_;
    new_input.ymax = input->ymax;
  }
  else {
    new_input.xmin = input->xmin;
    new_input.ymin = input->ymin - size_;
    new_input.xmax = input->xmax;
    new_input.ymax = input->ymax + size_;
  }

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void KeyingBlurOperation::get_area_of_interest(const int /*input_idx*/,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  switch (axis_) {
    case BLUR_AXIS_X:
      r_input_area.xmin = output_area.xmin - size_;
      r_input_area.ymin = output_area.ymin;
      r_input_area.xmax = output_area.xmax + size_;
      r_input_area.ymax = output_area.ymax;
      break;
    case BLUR_AXIS_Y:
      r_input_area.xmin = output_area.xmin;
      r_input_area.ymin = output_area.ymin - size_;
      r_input_area.xmax = output_area.xmax;
      r_input_area.ymax = output_area.ymax + size_;
      break;
    default:
      BLI_assert_msg(0, "Unknown axis");
      break;
  }
}

void KeyingBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  BuffersIterator<float> it = output->iterate_with(inputs, area);

  int coord_max;
  int elem_stride;
  std::function<int()> get_current_coord;
  switch (axis_) {
    case BLUR_AXIS_X:
      get_current_coord = [&] { return it.x; };
      coord_max = this->get_width();
      elem_stride = input->elem_stride;
      break;
    case BLUR_AXIS_Y:
      get_current_coord = [&] { return it.y; };
      coord_max = this->get_height();
      elem_stride = input->row_stride;
      break;
  }

  for (; !it.is_end(); ++it) {
    const int coord = get_current_coord();
    const int start_coord = MAX2(0, coord - size_ + 1);
    const int end_coord = MIN2(coord_max, coord + size_);
    const int count = end_coord - start_coord;

    float sum = 0.0f;
    const float *start = it.in(0) + (start_coord - coord) * elem_stride;
    const float *end = start + count * elem_stride;
    for (const float *elem = start; elem < end; elem += elem_stride) {
      sum += *elem;
    }

    *it.out = sum / count;
  }
}

}  // namespace blender::compositor
