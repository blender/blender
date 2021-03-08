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

MapRangeOperation::MapRangeOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_VALUE);
  this->m_inputOperation = nullptr;
  this->m_useClamp = false;
}

void MapRangeOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_sourceMinOperation = this->getInputSocketReader(1);
  this->m_sourceMaxOperation = this->getInputSocketReader(2);
  this->m_destMinOperation = this->getInputSocketReader(3);
  this->m_destMaxOperation = this->getInputSocketReader(4);
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

  this->m_inputOperation->readSampled(inputs, x, y, sampler);
  this->m_sourceMinOperation->readSampled(inputs + 1, x, y, sampler);
  this->m_sourceMaxOperation->readSampled(inputs + 2, x, y, sampler);
  this->m_destMinOperation->readSampled(inputs + 3, x, y, sampler);
  this->m_destMaxOperation->readSampled(inputs + 4, x, y, sampler);

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

  if (this->m_useClamp) {
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
  this->m_inputOperation = nullptr;
  this->m_sourceMinOperation = nullptr;
  this->m_sourceMaxOperation = nullptr;
  this->m_destMinOperation = nullptr;
  this->m_destMaxOperation = nullptr;
}
