/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector_types.hh"

#include "COM_CropOperation.h"

namespace blender::compositor {

CropBaseOperation::CropBaseOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Color);
  input_operation_ = nullptr;
  settings_ = nullptr;
  flags_.can_be_constant = true;
}

void CropBaseOperation::update_area()
{
  const NodeTwoXYs &node_two_xys = *settings_;
  const SocketReader *input = this->get_input_socket_reader(0);
  const int2 input_size = int2(input->get_width(), input->get_height());
  if (relative_) {
    /* The cropping bounds are relative to the image size. The factors are in the [0, 1] range,
     * so it is guaranteed that they won't go over the input image size. */
    xmin_ = input_size.x * node_two_xys.fac_x1;
    ymin_ = input_size.y * node_two_xys.fac_y2;
    xmax_ = input_size.x * node_two_xys.fac_x2;
    ymax_ = input_size.y * node_two_xys.fac_y1;
  }
  else {
    /* Make sure the bounds don't go over the input image size. */
    xmin_ = min_ii(node_two_xys.x1, input_size.x);
    ymin_ = min_ii(node_two_xys.y2, input_size.y);
    xmax_ = min_ii(node_two_xys.x2, input_size.x);
    ymax_ = min_ii(node_two_xys.y1, input_size.y);
  }

  /* Make sure upper bound is actually higher than the lower bound. */
  xmin_ = min_ii(xmin_, xmax_);
  ymin_ = min_ii(ymin_, ymax_);
  xmax_ = max_ii(xmin_, xmax_);
  ymax_ = max_ii(ymin_, ymax_);
}

void CropBaseOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
  update_area();
}

void CropBaseOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

CropOperation::CropOperation() : CropBaseOperation()
{
  /* pass */
}

void CropOperation::execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler)
{
  if ((x < xmax_ && x >= xmin_) && (y < ymax_ && y >= ymin_)) {
    input_operation_->read_sampled(output, x, y, sampler);
  }
  else {
    zero_v4(output);
  }
}

void CropOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    if ((it.x < xmax_ && it.x >= xmin_) && (it.y < ymax_ && it.y >= ymin_)) {
      copy_v4_v4(it.out, it.in(0));
    }
    else {
      zero_v4(it.out);
    }
  }
}

CropImageOperation::CropImageOperation() : CropBaseOperation()
{
  /* pass */
}

bool CropImageOperation::determine_depending_area_of_interest(rcti *input,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti new_input;

  new_input.xmax = input->xmax + xmin_;
  new_input.xmin = input->xmin + xmin_;
  new_input.ymax = input->ymax + ymin_;
  new_input.ymin = input->ymin + ymin_;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void CropImageOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmax = output_area.xmax + xmin_;
  r_input_area.xmin = output_area.xmin + xmin_;
  r_input_area.ymax = output_area.ymax + ymin_;
  r_input_area.ymin = output_area.ymin + ymin_;
}

void CropImageOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  update_area();
  r_area.xmax = r_area.xmin + (xmax_ - xmin_);
  r_area.ymax = r_area.ymin + (ymax_ - ymin_);
}

void CropImageOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  if (x >= 0 && x < get_width() && y >= 0 && y < get_height()) {
    input_operation_->read_sampled(output, (x + xmin_), (y + ymin_), sampler);
  }
  else {
    zero_v4(output);
  }
}

void CropImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const int width = get_width();
  const int height = get_height();
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    if (it.x >= 0 && it.x < width && it.y >= 0 && it.y < height) {
      input->read_elem_checked(it.x + xmin_, it.y + ymin_, it.out);
    }
    else {
      zero_v4(it.out);
    }
  }
}

}  // namespace blender::compositor
