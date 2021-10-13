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

#include "COM_GammaCorrectOperation.h"

namespace blender::compositor {

GammaCorrectOperation::GammaCorrectOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  inputProgram_ = nullptr;
  flags.can_be_constant = true;
}
void GammaCorrectOperation::initExecution()
{
  inputProgram_ = this->getInputSocketReader(0);
}

void GammaCorrectOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputColor[4];
  inputProgram_->readSampled(inputColor, x, y, sampler);
  if (inputColor[3] > 0.0f) {
    inputColor[0] /= inputColor[3];
    inputColor[1] /= inputColor[3];
    inputColor[2] /= inputColor[3];
  }

  /* check for negative to avoid nan's */
  output[0] = inputColor[0] > 0.0f ? inputColor[0] * inputColor[0] : 0.0f;
  output[1] = inputColor[1] > 0.0f ? inputColor[1] * inputColor[1] : 0.0f;
  output[2] = inputColor[2] > 0.0f ? inputColor[2] * inputColor[2] : 0.0f;
  output[3] = inputColor[3];

  if (inputColor[3] > 0.0f) {
    output[0] *= inputColor[3];
    output[1] *= inputColor[3];
    output[2] *= inputColor[3];
  }
}

void GammaCorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    /* Check for negative to avoid nan's. */
    it.out[0] = color[0] > 0.0f ? color[0] * color[0] : 0.0f;
    it.out[1] = color[1] > 0.0f ? color[1] * color[1] : 0.0f;
    it.out[2] = color[2] > 0.0f ? color[2] * color[2] : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

void GammaCorrectOperation::deinitExecution()
{
  inputProgram_ = nullptr;
}

GammaUncorrectOperation::GammaUncorrectOperation()
{
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  inputProgram_ = nullptr;
  flags.can_be_constant = true;
}
void GammaUncorrectOperation::initExecution()
{
  inputProgram_ = this->getInputSocketReader(0);
}

void GammaUncorrectOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float inputColor[4];
  inputProgram_->readSampled(inputColor, x, y, sampler);

  if (inputColor[3] > 0.0f) {
    inputColor[0] /= inputColor[3];
    inputColor[1] /= inputColor[3];
    inputColor[2] /= inputColor[3];
  }

  output[0] = inputColor[0] > 0.0f ? sqrtf(inputColor[0]) : 0.0f;
  output[1] = inputColor[1] > 0.0f ? sqrtf(inputColor[1]) : 0.0f;
  output[2] = inputColor[2] > 0.0f ? sqrtf(inputColor[2]) : 0.0f;
  output[3] = inputColor[3];

  if (inputColor[3] > 0.0f) {
    output[0] *= inputColor[3];
    output[1] *= inputColor[3];
    output[2] *= inputColor[3];
  }
}

void GammaUncorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    it.out[0] = color[0] > 0.0f ? sqrtf(color[0]) : 0.0f;
    it.out[1] = color[1] > 0.0f ? sqrtf(color[1]) : 0.0f;
    it.out[2] = color[2] > 0.0f ? sqrtf(color[2]) : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

void GammaUncorrectOperation::deinitExecution()
{
  inputProgram_ = nullptr;
}

}  // namespace blender::compositor
