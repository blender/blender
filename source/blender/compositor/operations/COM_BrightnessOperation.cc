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

namespace blender::compositor {

BrightnessOperation::BrightnessOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);
  inputProgram_ = nullptr;
  use_premultiply_ = false;
  flags.can_be_constant = true;
}

void BrightnessOperation::setUsePremultiply(bool use_premultiply)
{
  use_premultiply_ = use_premultiply;
}

void BrightnessOperation::initExecution()
{
  inputProgram_ = this->getInputSocketReader(0);
  inputBrightnessProgram_ = this->getInputSocketReader(1);
  inputContrastProgram_ = this->getInputSocketReader(2);
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
  inputProgram_->readSampled(inputValue, x, y, sampler);
  inputBrightnessProgram_->readSampled(inputBrightness, x, y, sampler);
  inputContrastProgram_->readSampled(inputContrast, x, y, sampler);
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
    a = 1.0f - delta * 2.0f;
    a = 1.0f / max_ff(a, FLT_EPSILON);
    b = a * (brightness - delta);
  }
  else {
    delta *= -1;
    a = max_ff(1.0f - delta * 2.0f, 0.0f);
    b = a * brightness + delta;
  }
  if (use_premultiply_) {
    premul_to_straight_v4(inputValue);
  }
  output[0] = a * inputValue[0] + b;
  output[1] = a * inputValue[1] + b;
  output[2] = a * inputValue[2] + b;
  output[3] = inputValue[3];
  if (use_premultiply_) {
    straight_to_premul_v4(output);
  }
}

void BrightnessOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  float tmp_color[4];
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_color = it.in(0);
    const float brightness = *it.in(1) / 100.0f;
    const float contrast = *it.in(2);
    float delta = contrast / 200.0f;
    /*
     * The algorithm is by Werner D. Streidt
     * (http://visca.com/ffactory/archives/5-99/msg00021.html)
     * Extracted of OpenCV demhist.c
     */
    float a, b;
    if (contrast > 0) {
      a = 1.0f - delta * 2.0f;
      a = 1.0f / max_ff(a, FLT_EPSILON);
      b = a * (brightness - delta);
    }
    else {
      delta *= -1;
      a = max_ff(1.0f - delta * 2.0f, 0.0f);
      b = a * brightness + delta;
    }
    const float *color;
    if (use_premultiply_) {
      premul_to_straight_v4_v4(tmp_color, in_color);
      color = tmp_color;
    }
    else {
      color = in_color;
    }
    it.out[0] = a * color[0] + b;
    it.out[1] = a * color[1] + b;
    it.out[2] = a * color[2] + b;
    it.out[3] = color[3];
    if (use_premultiply_) {
      straight_to_premul_v4(it.out);
    }
  }
}

void BrightnessOperation::deinitExecution()
{
  inputProgram_ = nullptr;
  inputBrightnessProgram_ = nullptr;
  inputContrastProgram_ = nullptr;
}

}  // namespace blender::compositor
