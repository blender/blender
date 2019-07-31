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

#include "COM_BrightnessOperation.h"

BrightnessOperation::BrightnessOperation() : NodeOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_VALUE);
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_inputProgram = NULL;
  this->m_use_premultiply = false;
}

void BrightnessOperation::setUsePremultiply(bool use_premultiply)
{
  this->m_use_premultiply = use_premultiply;
}

void BrightnessOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
  this->m_inputBrightnessProgram = this->getInputSocketReader(1);
  this->m_inputContrastProgram = this->getInputSocketReader(2);
}

void BrightnessOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputValue[4];
  float a, b;
  float inputBrightness[4];
  float inputContrast[4];
  this->m_inputProgram->readSampled(inputValue, x, y, sampler);
  this->m_inputBrightnessProgram->readSampled(inputBrightness, x, y, sampler);
  this->m_inputContrastProgram->readSampled(inputContrast, x, y, sampler);
  float brightness = inputBrightness[0];
  float contrast = inputContrast[0];
  brightness /= 100.0f;
  float delta = contrast / 200.0f;
  /*
   * The algorithm is by Werner D. Streidt
   * (http://visca.com/ffactory/archives/5-99/msg00021.html)
   * Extracted of OpenCV demhist.c
   */
  if (contrast > 0) {
    a = 1.0f / (1.0f - delta * 2.0f);
    b = a * (brightness - delta);
  }
  else {
    delta *= -1;
    a = 1.0f - delta * 2.0f;
    b = a * brightness + delta;
  }
  if (this->m_use_premultiply) {
    premul_to_straight_v4(inputValue);
  }
  output[0] = a * inputValue[0] + b;
  output[1] = a * inputValue[1] + b;
  output[2] = a * inputValue[2] + b;
  output[3] = inputValue[3];
  if (this->m_use_premultiply) {
    straight_to_premul_v4(output);
  }
}

void BrightnessOperation::deinitExecution()
{
  this->m_inputProgram = NULL;
  this->m_inputBrightnessProgram = NULL;
  this->m_inputContrastProgram = NULL;
}
