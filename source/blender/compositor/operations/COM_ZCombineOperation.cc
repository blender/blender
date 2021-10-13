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

#include "COM_ZCombineOperation.h"

namespace blender::compositor {

ZCombineOperation::ZCombineOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  image1Reader_ = nullptr;
  depth1Reader_ = nullptr;
  image2Reader_ = nullptr;
  depth2Reader_ = nullptr;
  this->flags.can_be_constant = true;
}

void ZCombineOperation::initExecution()
{
  image1Reader_ = this->getInputSocketReader(0);
  depth1Reader_ = this->getInputSocketReader(1);
  image2Reader_ = this->getInputSocketReader(2);
  depth2Reader_ = this->getInputSocketReader(3);
}

void ZCombineOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float depth1[4];
  float depth2[4];

  depth1Reader_->readSampled(depth1, x, y, sampler);
  depth2Reader_->readSampled(depth2, x, y, sampler);
  if (depth1[0] < depth2[0]) {
    image1Reader_->readSampled(output, x, y, sampler);
  }
  else {
    image2Reader_->readSampled(output, x, y, sampler);
  }
}

void ZCombineOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float depth1 = *it.in(1);
    const float depth2 = *it.in(3);
    const float *color = (depth1 < depth2) ? it.in(0) : it.in(2);
    copy_v4_v4(it.out, color);
  }
}

void ZCombineAlphaOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float depth1[4];
  float depth2[4];
  float color1[4];
  float color2[4];

  depth1Reader_->readSampled(depth1, x, y, sampler);
  depth2Reader_->readSampled(depth2, x, y, sampler);
  if (depth1[0] <= depth2[0]) {
    image1Reader_->readSampled(color1, x, y, sampler);
    image2Reader_->readSampled(color2, x, y, sampler);
  }
  else {
    image1Reader_->readSampled(color2, x, y, sampler);
    image2Reader_->readSampled(color1, x, y, sampler);
  }
  float fac = color1[3];
  float ifac = 1.0f - fac;
  output[0] = fac * color1[0] + ifac * color2[0];
  output[1] = fac * color1[1] + ifac * color2[1];
  output[2] = fac * color1[2] + ifac * color2[2];
  output[3] = MAX2(color1[3], color2[3]);
}

void ZCombineAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float depth1 = *it.in(1);
    const float depth2 = *it.in(3);
    const float *color1;
    const float *color2;
    if (depth1 <= depth2) {
      color1 = it.in(0);
      color2 = it.in(2);
    }
    else {
      color1 = it.in(2);
      color2 = it.in(0);
    }
    const float fac = color1[3];
    const float ifac = 1.0f - fac;
    it.out[0] = fac * color1[0] + ifac * color2[0];
    it.out[1] = fac * color1[1] + ifac * color2[1];
    it.out[2] = fac * color1[2] + ifac * color2[2];
    it.out[3] = MAX2(color1[3], color2[3]);
  }
}

void ZCombineOperation::deinitExecution()
{
  image1Reader_ = nullptr;
  depth1Reader_ = nullptr;
  image2Reader_ = nullptr;
  depth2Reader_ = nullptr;
}

// MASK combine
ZCombineMaskOperation::ZCombineMaskOperation()
{
  this->addInputSocket(DataType::Value);  // mask
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);

  maskReader_ = nullptr;
  image1Reader_ = nullptr;
  image2Reader_ = nullptr;
}

void ZCombineMaskOperation::initExecution()
{
  maskReader_ = this->getInputSocketReader(0);
  image1Reader_ = this->getInputSocketReader(1);
  image2Reader_ = this->getInputSocketReader(2);
}

void ZCombineMaskOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float mask[4];
  float color1[4];
  float color2[4];

  maskReader_->readSampled(mask, x, y, sampler);
  image1Reader_->readSampled(color1, x, y, sampler);
  image2Reader_->readSampled(color2, x, y, sampler);

  interp_v4_v4v4(output, color1, color2, 1.0f - mask[0]);
}

void ZCombineMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float mask = *it.in(0);
    const float *color1 = it.in(1);
    const float *color2 = it.in(2);
    interp_v4_v4v4(it.out, color1, color2, 1.0f - mask);
  }
}

void ZCombineMaskAlphaOperation::executePixelSampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float mask[4];
  float color1[4];
  float color2[4];

  maskReader_->readSampled(mask, x, y, sampler);
  image1Reader_->readSampled(color1, x, y, sampler);
  image2Reader_->readSampled(color2, x, y, sampler);

  float fac = (1.0f - mask[0]) * (1.0f - color1[3]) + mask[0] * color2[3];
  float mfac = 1.0f - fac;

  output[0] = color1[0] * mfac + color2[0] * fac;
  output[1] = color1[1] * mfac + color2[1] * fac;
  output[2] = color1[2] * mfac + color2[2] * fac;
  output[3] = MAX2(color1[3], color2[3]);
}

void ZCombineMaskAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float mask = *it.in(0);
    const float *color1 = it.in(1);
    const float *color2 = it.in(2);
    const float fac = (1.0f - mask) * (1.0f - color1[3]) + mask * color2[3];
    const float mfac = 1.0f - fac;

    it.out[0] = color1[0] * mfac + color2[0] * fac;
    it.out[1] = color1[1] * mfac + color2[1] * fac;
    it.out[2] = color1[2] * mfac + color2[2] * fac;
    it.out[3] = MAX2(color1[3], color2[3]);
  }
}

void ZCombineMaskOperation::deinitExecution()
{
  image1Reader_ = nullptr;
  maskReader_ = nullptr;
  image2Reader_ = nullptr;
}

}  // namespace blender::compositor
