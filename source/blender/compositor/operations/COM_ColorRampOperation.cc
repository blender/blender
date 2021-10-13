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

#include "COM_ColorRampOperation.h"

#include "BKE_colorband.h"

namespace blender::compositor {

ColorRampOperation::ColorRampOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  m_inputProgram = nullptr;
  m_colorBand = nullptr;
  this->flags.can_be_constant = true;
}
void ColorRampOperation::initExecution()
{
  m_inputProgram = this->getInputSocketReader(0);
}

void ColorRampOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float values[4];

  m_inputProgram->readSampled(values, x, y, sampler);
  BKE_colorband_evaluate(m_colorBand, values[0], output);
}

void ColorRampOperation::deinitExecution()
{
  m_inputProgram = nullptr;
}

void ColorRampOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    BKE_colorband_evaluate(m_colorBand, *it.in(0), it.out);
  }
}

}  // namespace blender::compositor
