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

#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

SetAlphaMultiplyOperation::SetAlphaMultiplyOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  m_inputColor = nullptr;
  m_inputAlpha = nullptr;
  this->flags.can_be_constant = true;
}

void SetAlphaMultiplyOperation::initExecution()
{
  m_inputColor = getInputSocketReader(0);
  m_inputAlpha = getInputSocketReader(1);
}

void SetAlphaMultiplyOperation::executePixelSampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float color_input[4];
  float alpha_input[4];

  m_inputColor->readSampled(color_input, x, y, sampler);
  m_inputAlpha->readSampled(alpha_input, x, y, sampler);

  mul_v4_v4fl(output, color_input, alpha_input[0]);
}

void SetAlphaMultiplyOperation::deinitExecution()
{
  m_inputColor = nullptr;
  m_inputAlpha = nullptr;
}

void SetAlphaMultiplyOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float alpha = *it.in(1);
    mul_v4_v4fl(it.out, color, alpha);
  }
}

}  // namespace blender::compositor
