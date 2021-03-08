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

#include "COM_SetAlphaReplaceOperation.h"

SetAlphaReplaceOperation::SetAlphaReplaceOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);

  this->m_inputColor = nullptr;
  this->m_inputAlpha = nullptr;
}

void SetAlphaReplaceOperation::initExecution()
{
  this->m_inputColor = getInputSocketReader(0);
  this->m_inputAlpha = getInputSocketReader(1);
}

void SetAlphaReplaceOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float alpha_input[4];

  this->m_inputColor->readSampled(output, x, y, sampler);
  this->m_inputAlpha->readSampled(alpha_input, x, y, sampler);
  output[3] = alpha_input[0];
}

void SetAlphaReplaceOperation::deinitExecution()
{
  this->m_inputColor = nullptr;
  this->m_inputAlpha = nullptr;
}
