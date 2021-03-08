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
 * Copyright 2020, Blender Foundation.
 */

#include "COM_ColorExposureOperation.h"

ExposureOperation::ExposureOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_inputProgram = nullptr;
}

void ExposureOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
  this->m_inputExposureProgram = this->getInputSocketReader(1);
}

void ExposureOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue[4];
  float inputExposure[4];
  this->m_inputProgram->readSampled(inputValue, x, y, sampler);
  this->m_inputExposureProgram->readSampled(inputExposure, x, y, sampler);
  const float exposure = pow(2, inputExposure[0]);

  output[0] = inputValue[0] * exposure;
  output[1] = inputValue[1] * exposure;
  output[2] = inputValue[2] * exposure;

  output[3] = inputValue[3];
}

void ExposureOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
  this->m_inputExposureProgram = nullptr;
}
