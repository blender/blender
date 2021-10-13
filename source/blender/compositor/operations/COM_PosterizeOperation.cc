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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_PosterizeOperation.h"

namespace blender::compositor {

PosterizeOperation::PosterizeOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  inputProgram_ = nullptr;
  inputStepsProgram_ = nullptr;
  flags.can_be_constant = true;
}

void PosterizeOperation::initExecution()
{
  inputProgram_ = this->getInputSocketReader(0);
  inputStepsProgram_ = this->getInputSocketReader(1);
}

void PosterizeOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue[4];
  float inputSteps[4];

  inputProgram_->readSampled(inputValue, x, y, sampler);
  inputStepsProgram_->readSampled(inputSteps, x, y, sampler);
  CLAMP(inputSteps[0], 2.0f, 1024.0f);
  const float steps_inv = 1.0f / inputSteps[0];

  output[0] = floor(inputValue[0] / steps_inv) * steps_inv;
  output[1] = floor(inputValue[1] / steps_inv) * steps_inv;
  output[2] = floor(inputValue[2] / steps_inv) * steps_inv;
  output[3] = inputValue[3];
}

void PosterizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_value = it.in(0);
    const float *in_steps = it.in(1);
    float steps = in_steps[0];
    CLAMP(steps, 2.0f, 1024.0f);
    const float steps_inv = 1.0f / steps;

    it.out[0] = floor(in_value[0] / steps_inv) * steps_inv;
    it.out[1] = floor(in_value[1] / steps_inv) * steps_inv;
    it.out[2] = floor(in_value[2] / steps_inv) * steps_inv;
    it.out[3] = in_value[3];
  }
}

void PosterizeOperation::deinitExecution()
{
  inputProgram_ = nullptr;
  inputStepsProgram_ = nullptr;
}

}  // namespace blender::compositor
