/* SPDX-FileCopyrightText: 2011 Blender Foundation
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

void FlipOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  if (execution_model_ == eExecutionModel::FullFrame) {
    rcti input_area = r_area;
    if (flip_x_) {
      const int width = BLI_rcti_size_x(&input_area) - 1;
      r_area.xmax = (width - input_area.xmin) + 1;
      r_area.xmin = (width - input_area.xmax) + 1;
    }
    if (flip_y_) {
      const int height = BLI_rcti_size_y(&input_area) - 1;
      r_area.ymax = (height - input_area.ymin) + 1;
      r_area.ymin = (height - input_area.ymax) + 1;
    }
  }
}

void FlipOperation::get_area_of_interest(const int input_idx,
                                         const rcti &output_area,
                                         rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  if (flip_x_) {
    const int w = int(this->get_width()) - 1;
    r_input_area.xmax = (w - output_area.xmin) + 1;
    r_input_area.xmin = (w - output_area.xmax) + 1;
  }
  else {
    r_input_area.xmin = output_area.xmin;
    r_input_area.xmax = output_area.xmax;
  }
  if (flip_y_) {
    const int h = int(this->get_height()) - 1;
    r_input_area.ymax = (h - output_area.ymin) + 1;
    r_input_area.ymin = (h - output_area.ymax) + 1;
  }
  else {
    r_input_area.ymin = output_area.ymin;
    r_input_area.ymax = output_area.ymax;
  }
}

void FlipOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  const int input_offset_x = input_img->get_rect().xmin;
  const int input_offset_y = input_img->get_rect().ymin;
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int nx = flip_x_ ? (int(this->get_width()) - 1) - it.x : it.x;
    const int ny = flip_y_ ? (int(this->get_height()) - 1) - it.y : it.y;
    input_img->read_elem(input_offset_x + nx, input_offset_y + ny, it.out);
  }
}

}  // namespace blender::compositor
