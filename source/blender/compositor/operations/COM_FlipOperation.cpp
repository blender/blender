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

#include "COM_FlipOperation.h"

FlipOperation::FlipOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = nullptr;
  this->m_flipX = true;
  this->m_flipY = false;
}
void FlipOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
}

void FlipOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
}

void FlipOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float nx = this->m_flipX ? ((int)this->getWidth() - 1) - x : x;
  float ny = this->m_flipY ? ((int)this->getHeight() - 1) - y : y;

  this->m_inputOperation->readSampled(output, nx, ny, sampler);
}

bool FlipOperation::determineDependingAreaOfInterest(rcti *input,
                                                     ReadBufferOperation *readOperation,
                                                     rcti *output)
{
  rcti newInput;

  if (this->m_flipX) {
    const int w = (int)this->getWidth() - 1;
    newInput.xmax = (w - input->xmin) + 1;
    newInput.xmin = (w - input->xmax) - 1;
  }
  else {
    newInput.xmin = input->xmin;
    newInput.xmax = input->xmax;
  }
  if (this->m_flipY) {
    const int h = (int)this->getHeight() - 1;
    newInput.ymax = (h - input->ymin) + 1;
    newInput.ymin = (h - input->ymax) - 1;
  }
  else {
    newInput.ymin = input->ymin;
    newInput.ymax = input->ymax;
  }

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
