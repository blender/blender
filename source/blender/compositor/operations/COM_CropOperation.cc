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
  this->addInputSocket(DataType::Color, ResizeMode::Align);
  this->addOutputSocket(DataType::Color);
  m_inputOperation = nullptr;
  m_settings = nullptr;
}

void CropBaseOperation::updateArea()
{
  SocketReader *inputReference = this->getInputSocketReader(0);
  float width = inputReference->getWidth();
  float height = inputReference->getHeight();
  NodeTwoXYs local_settings = *m_settings;

  if (width > 0.0f && height > 0.0f) {
    if (m_relative) {
      local_settings.x1 = width * local_settings.fac_x1;
      local_settings.x2 = width * local_settings.fac_x2;
      local_settings.y1 = height * local_settings.fac_y1;
      local_settings.y2 = height * local_settings.fac_y2;
    }
    if (width <= local_settings.x1 + 1) {
      local_settings.x1 = width - 1;
    }
    if (height <= local_settings.y1 + 1) {
      local_settings.y1 = height - 1;
    }
    if (width <= local_settings.x2 + 1) {
      local_settings.x2 = width - 1;
    }
    if (height <= local_settings.y2 + 1) {
      local_settings.y2 = height - 1;
    }

    m_xmax = MAX2(local_settings.x1, local_settings.x2) + 1;
    m_xmin = MIN2(local_settings.x1, local_settings.x2);
    m_ymax = MAX2(local_settings.y1, local_settings.y2) + 1;
    m_ymin = MIN2(local_settings.y1, local_settings.y2);
  }
  else {
    m_xmax = 0;
    m_xmin = 0;
    m_ymax = 0;
    m_ymin = 0;
  }
}

void CropBaseOperation::initExecution()
{
  m_inputOperation = this->getInputSocketReader(0);
  updateArea();
}

void CropBaseOperation::deinitExecution()
{
  m_inputOperation = nullptr;
}

CropOperation::CropOperation() : CropBaseOperation()
{
  /* pass */
}

void CropOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  if ((x < m_xmax && x >= m_xmin) && (y < m_ymax && y >= m_ymin)) {
    m_inputOperation->readSampled(output, x, y, sampler);
  }
  else {
    zero_v4(output);
  }
}

void CropOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> inputs)
{
  rcti crop_area;
  BLI_rcti_init(&crop_area, m_xmin, m_xmax, m_ymin, m_ymax);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    if (BLI_rcti_isect_pt(&crop_area, it.x, it.y)) {
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

bool CropImageOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;

  newInput.xmax = input->xmax + m_xmin;
  newInput.xmin = input->xmin + m_xmin;
  newInput.ymax = input->ymax + m_ymin;
  newInput.ymin = input->ymin + m_ymin;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void CropImageOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmax = output_area.xmax + m_xmin;
  r_input_area.xmin = output_area.xmin + m_xmin;
  r_input_area.ymax = output_area.ymax + m_ymin;
  r_input_area.ymin = output_area.ymin + m_ymin;
}

void CropImageOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  updateArea();
  r_area.xmax = r_area.xmin + (m_xmax - m_xmin);
  r_area.ymax = r_area.ymin + (m_ymax - m_ymin);
}

void CropImageOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  if (x >= 0 && x < getWidth() && y >= 0 && y < getHeight()) {
    m_inputOperation->readSampled(output, (x + m_xmin), (y + m_ymin), sampler);
  }
  else {
    zero_v4(output);
  }
}

void CropImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  rcti op_area;
  BLI_rcti_init(&op_area, 0, getWidth(), 0, getHeight());
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    if (BLI_rcti_isect_pt(&op_area, it.x, it.y)) {
      input->read_elem_checked(it.x + m_xmin, it.y + m_ymin, it.out);
    }
    else {
      zero_v4(it.out);
    }
  }
}

}  // namespace blender::compositor
