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

#include "COM_FlipOperation.h"

namespace blender::compositor {

FlipOperation::FlipOperation()
{
  this->addInputSocket(DataType::Color, ResizeMode::None);
  this->addOutputSocket(DataType::Color);
  this->set_canvas_input_index(0);
  inputOperation_ = nullptr;
  flipX_ = true;
  flipY_ = false;
}
void FlipOperation::initExecution()
{
  inputOperation_ = this->getInputSocketReader(0);
}

void FlipOperation::deinitExecution()
{
  inputOperation_ = nullptr;
}

void FlipOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float nx = flipX_ ? ((int)this->getWidth() - 1) - x : x;
  float ny = flipY_ ? ((int)this->getHeight() - 1) - y : y;

  inputOperation_->readSampled(output, nx, ny, sampler);
}

bool FlipOperation::determineDependingAreaOfInterest(rcti *input,
                                                     ReadBufferOperation *readOperation,
                                                     rcti *output)
{
  rcti newInput;

  if (flipX_) {
    const int w = (int)this->getWidth() - 1;
    newInput.xmax = (w - input->xmin) + 1;
    newInput.xmin = (w - input->xmax) - 1;
  }
  else {
    newInput.xmin = input->xmin;
    newInput.xmax = input->xmax;
  }
  if (flipY_) {
    const int h = (int)this->getHeight() - 1;
    newInput.ymax = (h - input->ymin) + 1;
    newInput.ymin = (h - input->ymax) - 1;
  }
  else {
    newInput.ymin = input->ymin;
    newInput.ymax = input->ymax;
  }

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void FlipOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  if (execution_model_ == eExecutionModel::FullFrame) {
    rcti input_area = r_area;
    if (flipX_) {
      const int width = BLI_rcti_size_x(&input_area) - 1;
      r_area.xmax = (width - input_area.xmin) + 1;
      r_area.xmin = (width - input_area.xmax) + 1;
    }
    if (flipY_) {
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
  if (flipX_) {
    const int w = (int)this->getWidth() - 1;
    r_input_area.xmax = (w - output_area.xmin) + 1;
    r_input_area.xmin = (w - output_area.xmax) + 1;
  }
  else {
    r_input_area.xmin = output_area.xmin;
    r_input_area.xmax = output_area.xmax;
  }
  if (flipY_) {
    const int h = (int)this->getHeight() - 1;
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
    const int nx = flipX_ ? ((int)this->getWidth() - 1) - it.x : it.x;
    const int ny = flipY_ ? ((int)this->getHeight() - 1) - it.y : it.y;
    input_img->read_elem(input_offset_x + nx, input_offset_y + ny, it.out);
  }
}

}  // namespace blender::compositor
