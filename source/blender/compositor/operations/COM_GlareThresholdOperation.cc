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

#include "COM_GlareThresholdOperation.h"
#include "BLI_math.h"

#include "IMB_colormanagement.h"

GlareThresholdOperation::GlareThresholdOperation()
{
  this->addInputSocket(COM_DT_COLOR, COM_SC_FIT);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_inputProgram = nullptr;
}

void GlareThresholdOperation::determineResolution(unsigned int resolution[2],
                                                  unsigned int preferredResolution[2])
{
  NodeOperation::determineResolution(resolution, preferredResolution);
  resolution[0] = resolution[0] / (1 << this->m_settings->quality);
  resolution[1] = resolution[1] / (1 << this->m_settings->quality);
}

void GlareThresholdOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
}

void GlareThresholdOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  const float threshold = this->m_settings->threshold;

  this->m_inputProgram->readSampled(output, x, y, sampler);
  if (IMB_colormanagement_get_luminance(output) >= threshold) {
    output[0] -= threshold;
    output[1] -= threshold;
    output[2] -= threshold;

    output[0] = MAX2(output[0], 0.0f);
    output[1] = MAX2(output[1], 0.0f);
    output[2] = MAX2(output[2], 0.0f);
  }
  else {
    zero_v3(output);
  }
}

void GlareThresholdOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
}
