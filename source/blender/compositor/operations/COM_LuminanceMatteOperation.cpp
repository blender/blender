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

#include "COM_LuminanceMatteOperation.h"
#include "BLI_math.h"

#include "IMB_colormanagement.h"

LuminanceMatteOperation::LuminanceMatteOperation()
{
  addInputSocket(COM_DT_COLOR);
  addOutputSocket(COM_DT_VALUE);

  this->m_inputImageProgram = nullptr;
}

void LuminanceMatteOperation::initExecution()
{
  this->m_inputImageProgram = this->getInputSocketReader(0);
}

void LuminanceMatteOperation::deinitExecution()
{
  this->m_inputImageProgram = nullptr;
}

void LuminanceMatteOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float inColor[4];
  this->m_inputImageProgram->readSampled(inColor, x, y, sampler);

  const float high = this->m_settings->t1;
  const float low = this->m_settings->t2;
  const float luminance = IMB_colormanagement_get_luminance(inColor);

  float alpha;

  /* one line thread-friend algorithm:
   * output[0] = MIN2(inputValue[3], MIN2(1.0f, MAX2(0.0f, ((luminance - low) / (high - low))));
   */

  /* test range */
  if (luminance > high) {
    alpha = 1.0f;
  }
  else if (luminance < low) {
    alpha = 0.0f;
  }
  else { /*blend */
    alpha = (luminance - low) / (high - low);
  }

  /* Store matte(alpha) value in [0] to go with
   * COM_SetAlphaMultiplyOperation and the Value output.
   */

  /* don't make something that was more transparent less transparent */
  output[0] = min_ff(alpha, inColor[3]);
}
