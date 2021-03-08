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

#include "COM_IDMaskOperation.h"

IDMaskOperation::IDMaskOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_VALUE);
  this->setComplex(true);
}

void *IDMaskOperation::initializeTileData(rcti *rect)
{
  void *buffer = getInputOperation(0)->initializeTileData(rect);
  return buffer;
}

void IDMaskOperation::executePixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const int buffer_width = input_buffer->getWidth();
  float *buffer = input_buffer->getBuffer();
  int buffer_index = (y * buffer_width + x);
  output[0] = (roundf(buffer[buffer_index]) == this->m_objectIndex) ? 1.0f : 0.0f;
}
