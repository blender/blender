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

#include "COM_InvertOperation.h"

namespace blender::compositor {

InvertOperation::InvertOperation()
{
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->m_inputValueProgram = nullptr;
  this->m_inputColorProgram = nullptr;
  this->m_color = true;
  this->m_alpha = false;
  set_canvas_input_index(1);
  this->flags.can_be_constant = true;
}
void InvertOperation::initExecution()
{
  this->m_inputValueProgram = this->getInputSocketReader(0);
  this->m_inputColorProgram = this->getInputSocketReader(1);
}

void InvertOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float inputValue[4];
  float inputColor[4];
  this->m_inputValueProgram->readSampled(inputValue, x, y, sampler);
  this->m_inputColorProgram->readSampled(inputColor, x, y, sampler);

  const float value = inputValue[0];
  const float invertedValue = 1.0f - value;

  if (this->m_color) {
    output[0] = (1.0f - inputColor[0]) * value + inputColor[0] * invertedValue;
    output[1] = (1.0f - inputColor[1]) * value + inputColor[1] * invertedValue;
    output[2] = (1.0f - inputColor[2]) * value + inputColor[2] * invertedValue;
  }
  else {
    copy_v3_v3(output, inputColor);
  }

  if (this->m_alpha) {
    output[3] = (1.0f - inputColor[3]) * value + inputColor[3] * invertedValue;
  }
  else {
    output[3] = inputColor[3];
  }
}

void InvertOperation::deinitExecution()
{
  this->m_inputValueProgram = nullptr;
  this->m_inputColorProgram = nullptr;
}

void InvertOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float value = *it.in(0);
    const float inverted_value = 1.0f - value;
    const float *color = it.in(1);

    if (this->m_color) {
      it.out[0] = (1.0f - color[0]) * value + color[0] * inverted_value;
      it.out[1] = (1.0f - color[1]) * value + color[1] * inverted_value;
      it.out[2] = (1.0f - color[2]) * value + color[2] * inverted_value;
    }
    else {
      copy_v3_v3(it.out, color);
    }

    if (this->m_alpha) {
      it.out[3] = (1.0f - color[3]) * value + color[3] * inverted_value;
    }
    else {
      it.out[3] = color[3];
    }
  }
}

}  // namespace blender::compositor
