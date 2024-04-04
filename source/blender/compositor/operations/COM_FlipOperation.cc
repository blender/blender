/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_FlipOperation.h"

namespace blender::compositor {

FlipOperation::FlipOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::None);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  flip_x_ = true;
  flip_y_ = false;
  flags_.can_be_constant = true;
}
void FlipOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void FlipOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

void FlipOperation::execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler)
{
  float nx = flip_x_ ? (int(this->get_width()) - 1) - x : x;
  float ny = flip_y_ ? (int(this->get_height()) - 1) - y : y;

  input_operation_->read_sampled(output, nx, ny, sampler);
}

bool FlipOperation::determine_depending_area_of_interest(rcti *input,
                                                         ReadBufferOperation *read_operation,
                                                         rcti *output)
{
  rcti new_input;

  if (flip_x_) {
    const int w = int(this->get_width()) - 1;
    new_input.xmax = (w - input->xmin) + 1;
    new_input.xmin = (w - input->xmax) - 1;
  }
  else {
    new_input.xmin = input->xmin;
    new_input.xmax = input->xmax;
  }
  if (flip_y_) {
    const int h = int(this->get_height()) - 1;
    new_input.ymax = (h - input->ymin) + 1;
    new_input.ymin = (h - input->ymax) - 1;
  }
  else {
    new_input.ymin = input->ymin;
    new_input.ymax = input->ymax;
  }

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void FlipOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  if (input_img->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input_img->get_elem(0, 0));
    return;
  }
  const int input_offset_x = input_img->get_rect().xmin;
  const int input_offset_y = input_img->get_rect().ymin;
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int nx = flip_x_ ? (int(this->get_width()) - 1) - it.x : it.x;
    const int ny = flip_y_ ? (int(this->get_height()) - 1) - it.y : it.y;
    input_img->read_elem(input_offset_x + nx, input_offset_y + ny, it.out);
  }
}

}  // namespace blender::compositor
