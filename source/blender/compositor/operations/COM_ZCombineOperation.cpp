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
#include "BLI_utildefines.h"

ZCombineOperation::ZCombineOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);

  this->m_image1Reader = nullptr;
  this->m_depth1Reader = nullptr;
  this->m_image2Reader = nullptr;
  this->m_depth2Reader = nullptr;
}

void ZCombineOperation::initExecution()
{
  this->m_image1Reader = this->getInputSocketReader(0);
  this->m_depth1Reader = this->getInputSocketReader(1);
  this->m_image2Reader = this->getInputSocketReader(2);
  this->m_depth2Reader = this->getInputSocketReader(3);
}

void ZCombineOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float depth1[4];
  float depth2[4];

  this->m_depth1Reader->readSampled(depth1, x, y, sampler);
  this->m_depth2Reader->readSampled(depth2, x, y, sampler);
  if (depth1[0] < depth2[0]) {
    this->m_image1Reader->readSampled(output, x, y, sampler);
  }
  else {
    this->m_image2Reader->readSampled(output, x, y, sampler);
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

  this->m_depth1Reader->readSampled(depth1, x, y, sampler);
  this->m_depth2Reader->readSampled(depth2, x, y, sampler);
  if (depth1[0] <= depth2[0]) {
    this->m_image1Reader->readSampled(color1, x, y, sampler);
    this->m_image2Reader->readSampled(color2, x, y, sampler);
  }
  else {
    this->m_image1Reader->readSampled(color2, x, y, sampler);
    this->m_image2Reader->readSampled(color1, x, y, sampler);
  }
  float fac = color1[3];
  float ifac = 1.0f - fac;
  output[0] = fac * color1[0] + ifac * color2[0];
  output[1] = fac * color1[1] + ifac * color2[1];
  output[2] = fac * color1[2] + ifac * color2[2];
  output[3] = MAX2(color1[3], color2[3]);
}

void ZCombineOperation::deinitExecution()
{
  this->m_image1Reader = nullptr;
  this->m_depth1Reader = nullptr;
  this->m_image2Reader = nullptr;
  this->m_depth2Reader = nullptr;
}

// MASK combine
ZCombineMaskOperation::ZCombineMaskOperation()
{
  this->addInputSocket(COM_DT_VALUE);  // mask
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_COLOR);

  this->m_maskReader = nullptr;
  this->m_image1Reader = nullptr;
  this->m_image2Reader = nullptr;
}

void ZCombineMaskOperation::initExecution()
{
  this->m_maskReader = this->getInputSocketReader(0);
  this->m_image1Reader = this->getInputSocketReader(1);
  this->m_image2Reader = this->getInputSocketReader(2);
}

void ZCombineMaskOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float mask[4];
  float color1[4];
  float color2[4];

  this->m_maskReader->readSampled(mask, x, y, sampler);
  this->m_image1Reader->readSampled(color1, x, y, sampler);
  this->m_image2Reader->readSampled(color2, x, y, sampler);

  interp_v4_v4v4(output, color1, color2, 1.0f - mask[0]);
}

void ZCombineMaskAlphaOperation::executePixelSampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float mask[4];
  float color1[4];
  float color2[4];

  this->m_maskReader->readSampled(mask, x, y, sampler);
  this->m_image1Reader->readSampled(color1, x, y, sampler);
  this->m_image2Reader->readSampled(color2, x, y, sampler);

  float fac = (1.0f - mask[0]) * (1.0f - color1[3]) + mask[0] * color2[3];
  float mfac = 1.0f - fac;

  output[0] = color1[0] * mfac + color2[0] * fac;
  output[1] = color1[1] * mfac + color2[1] * fac;
  output[2] = color1[2] * mfac + color2[2] * fac;
  output[3] = MAX2(color1[3], color2[3]);
}

void ZCombineMaskOperation::deinitExecution()
{
  this->m_image1Reader = nullptr;
  this->m_maskReader = nullptr;
  this->m_image2Reader = nullptr;
}
