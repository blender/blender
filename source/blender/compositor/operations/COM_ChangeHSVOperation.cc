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

#include "COM_ChangeHSVOperation.h"

namespace blender::compositor {

ChangeHSVOperation::ChangeHSVOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  m_inputOperation = nullptr;
  this->flags.can_be_constant = true;
}

void ChangeHSVOperation::initExecution()
{
  m_inputOperation = getInputSocketReader(0);
  m_hueOperation = getInputSocketReader(1);
  m_saturationOperation = getInputSocketReader(2);
  m_valueOperation = getInputSocketReader(3);
}

void ChangeHSVOperation::deinitExecution()
{
  m_inputOperation = nullptr;
  m_hueOperation = nullptr;
  m_saturationOperation = nullptr;
  m_valueOperation = nullptr;
}

void ChangeHSVOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputColor1[4];
  float hue[4], saturation[4], value[4];

  m_inputOperation->readSampled(inputColor1, x, y, sampler);
  m_hueOperation->readSampled(hue, x, y, sampler);
  m_saturationOperation->readSampled(saturation, x, y, sampler);
  m_valueOperation->readSampled(value, x, y, sampler);

  output[0] = inputColor1[0] + (hue[0] - 0.5f);
  if (output[0] > 1.0f) {
    output[0] -= 1.0f;
  }
  else if (output[0] < 0.0f) {
    output[0] += 1.0f;
  }
  output[1] = inputColor1[1] * saturation[0];
  output[2] = inputColor1[2] * value[0];
  output[3] = inputColor1[3];
}

void ChangeHSVOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float hue = *it.in(1);
    it.out[0] = color[0] + (hue - 0.5f);
    if (it.out[0] > 1.0f) {
      it.out[0] -= 1.0f;
    }
    else if (it.out[0] < 0.0f) {
      it.out[0] += 1.0f;
    }
    const float saturation = *it.in(2);
    const float value = *it.in(3);
    it.out[1] = color[1] * saturation;
    it.out[2] = color[2] * value;
    it.out[3] = color[3];
  }
}

}  // namespace blender::compositor
