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

#include "COM_ChromaMatteOperation.h"
#include "BLI_math.h"

ChromaMatteOperation::ChromaMatteOperation()
{
  addInputSocket(COM_DT_COLOR);
  addInputSocket(COM_DT_COLOR);
  addOutputSocket(COM_DT_VALUE);

  this->m_inputImageProgram = nullptr;
  this->m_inputKeyProgram = nullptr;
}

void ChromaMatteOperation::initExecution()
{
  this->m_inputImageProgram = this->getInputSocketReader(0);
  this->m_inputKeyProgram = this->getInputSocketReader(1);
}

void ChromaMatteOperation::deinitExecution()
{
  this->m_inputImageProgram = nullptr;
  this->m_inputKeyProgram = nullptr;
}

void ChromaMatteOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inKey[4];
  float inImage[4];

  const float acceptance = this->m_settings->t1; /* in radians */
  const float cutoff = this->m_settings->t2;     /* in radians */
  const float gain = this->m_settings->fstrength;

  float x_angle, z_angle, alpha;
  float theta, beta;
  float kfg;

  this->m_inputKeyProgram->readSampled(inKey, x, y, sampler);
  this->m_inputImageProgram->readSampled(inImage, x, y, sampler);

  /* Store matte(alpha) value in [0] to go with
   * #COM_SetAlphaMultiplyOperation and the Value output. */

  /* Algorithm from book "Video Demystified", does not include the spill reduction part. */
  /* Find theta, the angle that the color space should be rotated based on key. */

  /* rescale to -1.0..1.0 */
  // inImage[0] = (inImage[0] * 2.0f) - 1.0f;  // UNUSED
  inImage[1] = (inImage[1] * 2.0f) - 1.0f;
  inImage[2] = (inImage[2] * 2.0f) - 1.0f;

  // inKey[0] = (inKey[0] * 2.0f) - 1.0f;  // UNUSED
  inKey[1] = (inKey[1] * 2.0f) - 1.0f;
  inKey[2] = (inKey[2] * 2.0f) - 1.0f;

  theta = atan2(inKey[2], inKey[1]);

  /*rotate the cb and cr into x/z space */
  x_angle = inImage[1] * cosf(theta) + inImage[2] * sinf(theta);
  z_angle = inImage[2] * cosf(theta) - inImage[1] * sinf(theta);

  /*if within the acceptance angle */
  /* if kfg is <0 then the pixel is outside of the key color */
  kfg = x_angle - (fabsf(z_angle) / tanf(acceptance / 2.0f));

  if (kfg > 0.0f) { /* found a pixel that is within key color */
    alpha = 1.0f - (kfg / gain);

    beta = atan2(z_angle, x_angle);

    /* if beta is within the cutoff angle */
    if (fabsf(beta) < (cutoff / 2.0f)) {
      alpha = 0.0f;
    }

    /* don't make something that was more transparent less transparent */
    if (alpha < inImage[3]) {
      output[0] = alpha;
    }
    else {
      output[0] = inImage[3];
    }
  }
  else {                    /*pixel is outside key color */
    output[0] = inImage[3]; /* make pixel just as transparent as it was before */
  }
}
