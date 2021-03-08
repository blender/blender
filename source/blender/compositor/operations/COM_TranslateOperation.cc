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

#include "COM_TranslateOperation.h"

TranslateOperation::TranslateOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->setResolutionInputSocketIndex(0);
  this->m_inputOperation = nullptr;
  this->m_inputXOperation = nullptr;
  this->m_inputYOperation = nullptr;
  this->m_isDeltaSet = false;
  this->m_factorX = 1.0f;
  this->m_factorY = 1.0f;
}
void TranslateOperation::initExecution()
{
  this->m_inputOperation = this->getInputSocketReader(0);
  this->m_inputXOperation = this->getInputSocketReader(1);
  this->m_inputYOperation = this->getInputSocketReader(2);
}

void TranslateOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
  this->m_inputXOperation = nullptr;
  this->m_inputYOperation = nullptr;
}

void TranslateOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler /*sampler*/)
{
  ensureDelta();

  float originalXPos = x - this->getDeltaX();
  float originalYPos = y - this->getDeltaY();

  this->m_inputOperation->readSampled(output, originalXPos, originalYPos, COM_PS_BILINEAR);
}

bool TranslateOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;

  ensureDelta();

  newInput.xmin = input->xmin - this->getDeltaX();
  newInput.xmax = input->xmax - this->getDeltaX();
  newInput.ymin = input->ymin - this->getDeltaY();
  newInput.ymax = input->ymax - this->getDeltaY();

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void TranslateOperation::setFactorXY(float factorX, float factorY)
{
  m_factorX = factorX;
  m_factorY = factorY;
}
