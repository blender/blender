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

#include "COM_TranslateOperation.h"

namespace blender::compositor {

TranslateOperation::TranslateOperation() : TranslateOperation(DataType::Color)
{
}
TranslateOperation::TranslateOperation(DataType data_type)
{
  this->addInputSocket(data_type);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(data_type);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = nullptr;
  this->m_inputXOperation = nullptr;
  this->m_inputYOperation = nullptr;
  this->m_isDeltaSet = false;
  this->m_factorX = 1.0f;
  this->m_factorY = 1.0f;
  this->x_extend_mode_ = MemoryBufferExtend::Clip;
  this->y_extend_mode_ = MemoryBufferExtend::Clip;
}
void TranslateOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_inputXOperation = this->getInputSocketReader(1);
  this->m_inputYOperation = this->getInputSocketReader(2);
}

void TranslateOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
  this->m_inputXOperation = nullptr;
  this->m_inputYOperation = nullptr;
}

void TranslateOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler /*sampler*/)
{
  ensureDelta();

  float originalXPos = x - this->getDeltaX();
  float originalYPos = y - this->getDeltaY();

  this->m_inputOperation->readSampled(output, originalXPos, originalYPos, PixelSampler::Bilinear);
}

bool TranslateOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;

  ensureDelta();

  newInput.xmin = input->xmin - this->getDeltaX();
  newInput.xmax = input->xmax - this->getDeltaX();
  newInput.ymin = input->ymin - this->getDeltaY();
  newInput.ymax = input->ymax - this->getDeltaY();

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void TranslateOperation::setFactorXY(float factorX, float factorY)
{
  m_factorX = factorX;
  m_factorY = factorY;
}

void TranslateOperation::set_wrapping(int wrapping_type)
{
  switch (wrapping_type) {
    case CMP_NODE_WRAP_X:
      x_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_Y:
      y_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_XY:
      x_extend_mode_ = MemoryBufferExtend::Repeat;
      y_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    default:
      break;
  }
}

void TranslateOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  if (input_idx == 0) {
    ensureDelta();
    r_input_area = output_area;
    if (x_extend_mode_ == MemoryBufferExtend::Clip) {
      const int delta_x = this->getDeltaX();
      BLI_rcti_translate(&r_input_area, -delta_x, 0);
    }
    if (y_extend_mode_ == MemoryBufferExtend::Clip) {
      const int delta_y = this->getDeltaY();
      BLI_rcti_translate(&r_input_area, 0, -delta_y);
    }
  }
}

void TranslateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];
  const int delta_x = this->getDeltaX();
  const int delta_y = this->getDeltaY();
  for (int y = area.ymin; y < area.ymax; y++) {
    float *out = output->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      const int input_x = x - delta_x;
      const int input_y = y - delta_y;
      input->read(out, input_x, input_y, x_extend_mode_, y_extend_mode_);
      out += output->elem_stride;
    }
  }
}

}  // namespace blender::compositor
