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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_KeyingBlurOperation.h"

namespace blender::compositor {

KeyingBlurOperation::KeyingBlurOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);

  m_size = 0;
  m_axis = BLUR_AXIS_X;

  this->flags.complex = true;
}

void *KeyingBlurOperation::initializeTileData(rcti *rect)
{
  void *buffer = getInputOperation(0)->initializeTileData(rect);

  return buffer;
}

void KeyingBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  const int bufferWidth = inputBuffer->getWidth();
  float *buffer = inputBuffer->getBuffer();
  int count = 0;
  float average = 0.0f;

  if (m_axis == 0) {
    const int start = MAX2(0, x - m_size + 1), end = MIN2(bufferWidth, x + m_size);
    for (int cx = start; cx < end; cx++) {
      int bufferIndex = (y * bufferWidth + cx);
      average += buffer[bufferIndex];
      count++;
    }
  }
  else {
    const int start = MAX2(0, y - m_size + 1), end = MIN2(inputBuffer->getHeight(), y + m_size);
    for (int cy = start; cy < end; cy++) {
      int bufferIndex = (cy * bufferWidth + x);
      average += buffer[bufferIndex];
      count++;
    }
  }

  average /= (float)count;

  output[0] = average;
}

bool KeyingBlurOperation::determineDependingAreaOfInterest(rcti *input,
                                                           ReadBufferOperation *readOperation,
                                                           rcti *output)
{
  rcti newInput;

  if (m_axis == BLUR_AXIS_X) {
    newInput.xmin = input->xmin - m_size;
    newInput.ymin = input->ymin;
    newInput.xmax = input->xmax + m_size;
    newInput.ymax = input->ymax;
  }
  else {
    newInput.xmin = input->xmin;
    newInput.ymin = input->ymin - m_size;
    newInput.xmax = input->xmax;
    newInput.ymax = input->ymax + m_size;
  }

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void KeyingBlurOperation::get_area_of_interest(const int UNUSED(input_idx),
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  switch (m_axis) {
    case BLUR_AXIS_X:
      r_input_area.xmin = output_area.xmin - m_size;
      r_input_area.ymin = output_area.ymin;
      r_input_area.xmax = output_area.xmax + m_size;
      r_input_area.ymax = output_area.ymax;
      break;
    case BLUR_AXIS_Y:
      r_input_area.xmin = output_area.xmin;
      r_input_area.ymin = output_area.ymin - m_size;
      r_input_area.xmax = output_area.xmax;
      r_input_area.ymax = output_area.ymax + m_size;
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
  switch (m_axis) {
    case BLUR_AXIS_X:
      get_current_coord = [&] { return it.x; };
      coord_max = this->getWidth();
      elem_stride = input->elem_stride;
      break;
    case BLUR_AXIS_Y:
      get_current_coord = [&] { return it.y; };
      coord_max = this->getHeight();
      elem_stride = input->row_stride;
      break;
  }

  for (; !it.is_end(); ++it) {
    const int coord = get_current_coord();
    const int start_coord = MAX2(0, coord - m_size + 1);
    const int end_coord = MIN2(coord_max, coord + m_size);
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
