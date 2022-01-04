/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_CropOperation.h"

namespace blender::compositor {

CropBaseOperation::CropBaseOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Color);
  input_operation_ = nullptr;
  settings_ = nullptr;
}

void CropBaseOperation::update_area()
{
  SocketReader *input_reference = this->get_input_socket_reader(0);
  float width = input_reference->get_width();
  float height = input_reference->get_height();
  NodeTwoXYs local_settings = *settings_;

  if (width > 0.0f && height > 0.0f) {
    if (relative_) {
      local_settings.x1 = width * local_settings.fac_x1;
      local_settings.x2 = width * local_settings.fac_x2;
      local_settings.y1 = height * local_settings.fac_y1;
      local_settings.y2 = height * local_settings.fac_y2;
    }
    if (width < local_settings.x1) {
      local_settings.x1 = width;
    }
    if (height < local_settings.y1) {
      local_settings.y1 = height;
    }
    if (width < local_settings.x2) {
      local_settings.x2 = width;
    }
    if (height < local_settings.y2) {
      local_settings.y2 = height;
    }

    xmax_ = MAX2(local_settings.x1, local_settings.x2);
    xmin_ = MIN2(local_settings.x1, local_settings.x2);
    ymax_ = MAX2(local_settings.y1, local_settings.y2);
    ymin_ = MIN2(local_settings.y1, local_settings.y2);
  }
  else {
    xmax_ = 0;
    xmin_ = 0;
    ymax_ = 0;
    ymin_ = 0;
  }
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
