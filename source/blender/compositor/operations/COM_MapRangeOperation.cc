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

#include "COM_MapRangeOperation.h"

namespace blender::compositor {

MapRangeOperation::MapRangeOperation()
{
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  inputOperation_ = nullptr;
  useClamp_ = false;
  flags.can_be_constant = true;
}

void MapRangeOperation::initExecution()
{
  inputOperation_ = this->getInputSocketReader(0);
  sourceMinOperation_ = this->getInputSocketReader(1);
  sourceMaxOperation_ = this->getInputSocketReader(2);
  destMinOperation_ = this->getInputSocketReader(3);
  destMaxOperation_ = this->getInputSocketReader(4);
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel
 */
#define BLENDER_ZMAX 10000.0f

void MapRangeOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputs[8]; /* includes the 5 inputs + 3 pads */
  float value;
  float source_min, source_max;
  float dest_min, dest_max;

  inputOperation_->readSampled(inputs, x, y, sampler);
  sourceMinOperation_->readSampled(inputs + 1, x, y, sampler);
  sourceMaxOperation_->readSampled(inputs + 2, x, y, sampler);
  destMinOperation_->readSampled(inputs + 3, x, y, sampler);
  destMaxOperation_->readSampled(inputs + 4, x, y, sampler);

  value = inputs[0];
  source_min = inputs[1];
  source_max = inputs[2];
  dest_min = inputs[3];
  dest_max = inputs[4];

  if (fabsf(source_max - source_min) < 1e-6f) {
    output[0] = 0.0f;
    return;
  }

  if (value >= -BLENDER_ZMAX && value <= BLENDER_ZMAX) {
    value = (value - source_min) / (source_max - source_min);
    value = dest_min + value * (dest_max - dest_min);
  }
  else if (value > BLENDER_ZMAX) {
    value = dest_max;
  }
  else {
    value = dest_min;
  }

  if (useClamp_) {
    if (dest_max > dest_min) {
      CLAMP(value, dest_min, dest_max);
    }
    else {
      CLAMP(value, dest_max, dest_min);
    }
  }

  output[0] = value;
}

void MapRangeOperation::deinitExecution()
{
  inputOperation_ = nullptr;
  sourceMinOperation_ = nullptr;
  sourceMaxOperation_ = nullptr;
  destMinOperation_ = nullptr;
  destMaxOperation_ = nullptr;
}

void MapRangeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float source_min = *it.in(1);
    const float source_max = *it.in(2);
    if (fabsf(source_max - source_min) < 1e-6f) {
      it.out[0] = 0.0f;
      continue;
    }

    float value = *it.in(0);
    const float dest_min = *it.in(3);
    const float dest_max = *it.in(4);
    if (value >= -BLENDER_ZMAX && value <= BLENDER_ZMAX) {
      value = (value - source_min) / (source_max - source_min);
      value = dest_min + value * (dest_max - dest_min);
    }
    else if (value > BLENDER_ZMAX) {
      value = dest_max;
    }
    else {
      value = dest_min;
    }

    if (useClamp_) {
      if (dest_max > dest_min) {
        CLAMP(value, dest_min, dest_max);
      }
      else {
        CLAMP(value, dest_max, dest_min);
      }
    }

    it.out[0] = value;
  }
}

}  // namespace blender::compositor
