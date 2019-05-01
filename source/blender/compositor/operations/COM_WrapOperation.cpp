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

#include "COM_WrapOperation.h"

WrapOperation::WrapOperation(DataType datatype) : ReadBufferOperation(datatype)
{
  this->m_wrappingType = CMP_NODE_WRAP_NONE;
}

inline float WrapOperation::getWrappedOriginalXPos(float x)
{
  if (this->getWidth() == 0) {
    return 0;
  }
  while (x < 0) {
    x += this->m_width;
  }
  return fmodf(x, this->getWidth());
}

inline float WrapOperation::getWrappedOriginalYPos(float y)
{
  if (this->getHeight() == 0) {
    return 0;
  }
  while (y < 0) {
    y += this->m_height;
  }
  return fmodf(y, this->getHeight());
}

void WrapOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float nx, ny;
  nx = x;
  ny = y;
  MemoryBufferExtend extend_x = COM_MB_CLIP, extend_y = COM_MB_CLIP;
  switch (m_wrappingType) {
    case CMP_NODE_WRAP_NONE:
      // Intentionally empty, originalXPos and originalYPos have been set before
      break;
    case CMP_NODE_WRAP_X:
      // wrap only on the x-axis
      nx = this->getWrappedOriginalXPos(x);
      extend_x = COM_MB_REPEAT;
      break;
    case CMP_NODE_WRAP_Y:
      // wrap only on the y-axis
      ny = this->getWrappedOriginalYPos(y);
      extend_y = COM_MB_REPEAT;
      break;
    case CMP_NODE_WRAP_XY:
      // wrap on both
      nx = this->getWrappedOriginalXPos(x);
      ny = this->getWrappedOriginalYPos(y);
      extend_x = COM_MB_REPEAT;
      extend_y = COM_MB_REPEAT;
      break;
  }

  executePixelExtend(output, nx, ny, sampler, extend_x, extend_y);
}

bool WrapOperation::determineDependingAreaOfInterest(rcti *input,
                                                     ReadBufferOperation *readOperation,
                                                     rcti *output)
{
  rcti newInput;
  newInput.xmin = input->xmin;
  newInput.xmax = input->xmax;
  newInput.ymin = input->ymin;
  newInput.ymax = input->ymax;

  if (m_wrappingType == CMP_NODE_WRAP_X || m_wrappingType == CMP_NODE_WRAP_XY) {
    // wrap only on the x-axis if tile is wrapping
    newInput.xmin = getWrappedOriginalXPos(input->xmin);
    newInput.xmax = getWrappedOriginalXPos(input->xmax) + 0.5f;
    if (newInput.xmin >= newInput.xmax) {
      newInput.xmin = 0;
      newInput.xmax = this->getWidth();
    }
  }
  if (m_wrappingType == CMP_NODE_WRAP_Y || m_wrappingType == CMP_NODE_WRAP_XY) {
    // wrap only on the y-axis if tile is wrapping
    newInput.ymin = getWrappedOriginalYPos(input->ymin);
    newInput.ymax = getWrappedOriginalYPos(input->ymax) + 0.5f;
    if (newInput.ymin >= newInput.ymax) {
      newInput.ymin = 0;
      newInput.ymax = this->getHeight();
    }
  }

  return ReadBufferOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void WrapOperation::setWrapping(int wrapping_type)
{
  m_wrappingType = wrapping_type;
}
