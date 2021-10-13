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

#include "COM_MixOperation.h"

namespace blender::compositor {

/* ******** Mix Base Operation ******** */

MixBaseOperation::MixBaseOperation()
{
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->m_inputValueOperation = nullptr;
  this->m_inputColor1Operation = nullptr;
  this->m_inputColor2Operation = nullptr;
  this->setUseValueAlphaMultiply(false);
  this->setUseClamp(false);
  flags.can_be_constant = true;
}

void MixBaseOperation::initExecution()
{
  this->m_inputValueOperation = this->getInputSocketReader(0);
  this->m_inputColor1Operation = this->getInputSocketReader(1);
  this->m_inputColor2Operation = this->getInputSocketReader(2);
}

void MixBaseOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * (inputColor1[0]) + value * (inputColor2[0]);
  output[1] = valuem * (inputColor1[1]) + value * (inputColor2[1]);
  output[2] = valuem * (inputColor1[2]) + value * (inputColor2[2]);
  output[3] = inputColor1[3];
}

void MixBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperationInput *socket;
  rcti temp_area;

  socket = this->getInputSocket(1);
  bool determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
  if (determined) {
    this->set_canvas_input_index(1);
  }
  else {
    socket = this->getInputSocket(2);
    determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
    if (determined) {
      this->set_canvas_input_index(2);
    }
    else {
      this->set_canvas_input_index(0);
    }
  }
  NodeOperation::determine_canvas(preferred_area, r_area);
}

void MixBaseOperation::deinitExecution()
{
  this->m_inputValueOperation = nullptr;
  this->m_inputColor1Operation = nullptr;
  this->m_inputColor2Operation = nullptr;
}

void MixBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                    const rcti &area,
                                                    Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_value = inputs[0];
  const MemoryBuffer *input_color1 = inputs[1];
  const MemoryBuffer *input_color2 = inputs[2];
  const int width = BLI_rcti_size_x(&area);
  PixelCursor p;
  p.out_stride = output->elem_stride;
  p.value_stride = input_value->elem_stride;
  p.color1_stride = input_color1->elem_stride;
  p.color2_stride = input_color2->elem_stride;
  for (int y = area.ymin; y < area.ymax; y++) {
    p.out = output->get_elem(area.xmin, y);
    p.row_end = p.out + width * output->elem_stride;
    p.value = input_value->get_elem(area.xmin, y);
    p.color1 = input_color1->get_elem(area.xmin, y);
    p.color2 = input_color2->get_elem(area.xmin, y);
    update_memory_buffer_row(p);
  }
}

void MixBaseOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = value_m * p.color1[0] + value * p.color2[0];
    p.out[1] = value_m * p.color1[1] + value * p.color2[1];
    p.out[2] = value_m * p.color1[2] + value * p.color2[2];
    p.out[3] = p.color1[3];
    p.next();
  }
}

/* ******** Mix Add Operation ******** */

void MixAddOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  output[0] = inputColor1[0] + value * inputColor2[0];
  output[1] = inputColor1[1] + value * inputColor2[1];
  output[2] = inputColor1[2] + value * inputColor2[2];
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixAddOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    p.out[0] = p.color1[0] + value * p.color2[0];
    p.out[1] = p.color1[1] + value * p.color2[1];
    p.out[2] = p.color1[2] + value * p.color2[2];
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Blend Operation ******** */

void MixBlendOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];
  float value;

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
  value = inputValue[0];

  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * (inputColor1[0]) + value * (inputColor2[0]);
  output[1] = valuem * (inputColor1[1]) + value * (inputColor2[1]);
  output[2] = valuem * (inputColor1[2]) + value * (inputColor2[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixBlendOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;
    p.out[0] = value_m * p.color1[0] + value * p.color2[0];
    p.out[1] = value_m * p.color1[1] + value * p.color2[1];
    p.out[2] = value_m * p.color1[2] + value * p.color2[2];
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Burn Operation ******** */

void MixColorBurnOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];
  float tmp;

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  tmp = valuem + value * inputColor2[0];
  if (tmp <= 0.0f) {
    output[0] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - inputColor1[0]) / tmp;
    if (tmp < 0.0f) {
      output[0] = 0.0f;
    }
    else if (tmp > 1.0f) {
      output[0] = 1.0f;
    }
    else {
      output[0] = tmp;
    }
  }

  tmp = valuem + value * inputColor2[1];
  if (tmp <= 0.0f) {
    output[1] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - inputColor1[1]) / tmp;
    if (tmp < 0.0f) {
      output[1] = 0.0f;
    }
    else if (tmp > 1.0f) {
      output[1] = 1.0f;
    }
    else {
      output[1] = tmp;
    }
  }

  tmp = valuem + value * inputColor2[2];
  if (tmp <= 0.0f) {
    output[2] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - inputColor1[2]) / tmp;
    if (tmp < 0.0f) {
      output[2] = 0.0f;
    }
    else if (tmp > 1.0f) {
      output[2] = 1.0f;
    }
    else {
      output[2] = tmp;
    }
  }

  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixColorBurnOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    float tmp = value_m + value * p.color2[0];
    if (tmp <= 0.0f) {
      p.out[0] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[0]) / tmp;
      p.out[0] = CLAMPIS(tmp, 0.0f, 1.0f);
    }

    tmp = value_m + value * p.color2[1];
    if (tmp <= 0.0f) {
      p.out[1] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[1]) / tmp;
      p.out[1] = CLAMPIS(tmp, 0.0f, 1.0f);
    }

    tmp = value_m + value * p.color2[2];
    if (tmp <= 0.0f) {
      p.out[2] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[2]) / tmp;
      p.out[2] = CLAMPIS(tmp, 0.0f, 1.0f);
    }
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Color Operation ******** */

void MixColorOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  float colH, colS, colV;
  rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
  if (colS != 0.0f) {
    float rH, rS, rV;
    float tmpr, tmpg, tmpb;
    rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
    hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
    output[0] = (valuem * inputColor1[0]) + (value * tmpr);
    output[1] = (valuem * inputColor1[1]) + (value * tmpg);
    output[2] = (valuem * inputColor1[2]) + (value * tmpb);
  }
  else {
    copy_v3_v3(output, inputColor1);
  }
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixColorOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    float colH, colS, colV;
    rgb_to_hsv(p.color2[0], p.color2[1], p.color2[2], &colH, &colS, &colV);
    if (colS != 0.0f) {
      float rH, rS, rV;
      float tmpr, tmpg, tmpb;
      rgb_to_hsv(p.color1[0], p.color1[1], p.color1[2], &rH, &rS, &rV);
      hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
      p.out[0] = (value_m * p.color1[0]) + (value * tmpr);
      p.out[1] = (value_m * p.color1[1]) + (value * tmpg);
      p.out[2] = (value_m * p.color1[2]) + (value * tmpb);
    }
    else {
      copy_v3_v3(p.out, p.color1);
    }
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Darken Operation ******** */

void MixDarkenOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  output[0] = min_ff(inputColor1[0], inputColor2[0]) * value + inputColor1[0] * valuem;
  output[1] = min_ff(inputColor1[1], inputColor2[1]) * value + inputColor1[1] * valuem;
  output[2] = min_ff(inputColor1[2], inputColor2[2]) * value + inputColor1[2] * valuem;
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixDarkenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;
    p.out[0] = min_ff(p.color1[0], p.color2[0]) * value + p.color1[0] * value_m;
    p.out[1] = min_ff(p.color1[1], p.color2[1]) * value + p.color1[1] * value_m;
    p.out[2] = min_ff(p.color1[2], p.color2[2]) * value + p.color1[2] * value_m;
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Difference Operation ******** */

void MixDifferenceOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * inputColor1[0] + value * fabsf(inputColor1[0] - inputColor2[0]);
  output[1] = valuem * inputColor1[1] + value * fabsf(inputColor1[1] - inputColor2[1]);
  output[2] = valuem * inputColor1[2] + value * fabsf(inputColor1[2] - inputColor2[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixDifferenceOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = value_m * p.color1[0] + value * fabsf(p.color1[0] - p.color2[0]);
    p.out[1] = value_m * p.color1[1] + value * fabsf(p.color1[1] - p.color2[1]);
    p.out[2] = value_m * p.color1[2] + value * fabsf(p.color1[2] - p.color2[2]);
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Difference Operation ******** */

void MixDivideOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  if (inputColor2[0] != 0.0f) {
    output[0] = valuem * (inputColor1[0]) + value * (inputColor1[0]) / inputColor2[0];
  }
  else {
    output[0] = 0.0f;
  }
  if (inputColor2[1] != 0.0f) {
    output[1] = valuem * (inputColor1[1]) + value * (inputColor1[1]) / inputColor2[1];
  }
  else {
    output[1] = 0.0f;
  }
  if (inputColor2[2] != 0.0f) {
    output[2] = valuem * (inputColor1[2]) + value * (inputColor1[2]) / inputColor2[2];
  }
  else {
    output[2] = 0.0f;
  }

  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixDivideOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    if (p.color2[0] != 0.0f) {
      p.out[0] = value_m * (p.color1[0]) + value * (p.color1[0]) / p.color2[0];
    }
    else {
      p.out[0] = 0.0f;
    }
    if (p.color2[1] != 0.0f) {
      p.out[1] = value_m * (p.color1[1]) + value * (p.color1[1]) / p.color2[1];
    }
    else {
      p.out[1] = 0.0f;
    }
    if (p.color2[2] != 0.0f) {
      p.out[2] = value_m * (p.color1[2]) + value * (p.color1[2]) / p.color2[2];
    }
    else {
      p.out[2] = 0.0f;
    }

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Dodge Operation ******** */

void MixDodgeOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];
  float tmp;

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }

  if (inputColor1[0] != 0.0f) {
    tmp = 1.0f - value * inputColor2[0];
    if (tmp <= 0.0f) {
      output[0] = 1.0f;
    }
    else {
      tmp = inputColor1[0] / tmp;
      if (tmp > 1.0f) {
        output[0] = 1.0f;
      }
      else {
        output[0] = tmp;
      }
    }
  }
  else {
    output[0] = 0.0f;
  }

  if (inputColor1[1] != 0.0f) {
    tmp = 1.0f - value * inputColor2[1];
    if (tmp <= 0.0f) {
      output[1] = 1.0f;
    }
    else {
      tmp = inputColor1[1] / tmp;
      if (tmp > 1.0f) {
        output[1] = 1.0f;
      }
      else {
        output[1] = tmp;
      }
    }
  }
  else {
    output[1] = 0.0f;
  }

  if (inputColor1[2] != 0.0f) {
    tmp = 1.0f - value * inputColor2[2];
    if (tmp <= 0.0f) {
      output[2] = 1.0f;
    }
    else {
      tmp = inputColor1[2] / tmp;
      if (tmp > 1.0f) {
        output[2] = 1.0f;
      }
      else {
        output[2] = tmp;
      }
    }
  }
  else {
    output[2] = 0.0f;
  }

  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixDodgeOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }

    float tmp;
    if (p.color1[0] != 0.0f) {
      tmp = 1.0f - value * p.color2[0];
      if (tmp <= 0.0f) {
        p.out[0] = 1.0f;
      }
      else {
        p.out[0] = p.color1[0] / tmp;
        CLAMP_MAX(p.out[0], 1.0f);
      }
    }
    else {
      p.out[0] = 0.0f;
    }

    if (p.color1[1] != 0.0f) {
      tmp = 1.0f - value * p.color2[1];
      if (tmp <= 0.0f) {
        p.out[1] = 1.0f;
      }
      else {
        p.out[1] = p.color1[1] / tmp;
        CLAMP_MAX(p.out[1], 1.0f);
      }
    }
    else {
      p.out[1] = 0.0f;
    }

    if (p.color1[2] != 0.0f) {
      tmp = 1.0f - value * p.color2[2];
      if (tmp <= 0.0f) {
        p.out[2] = 1.0f;
      }
      else {
        p.out[2] = p.color1[2] / tmp;
        CLAMP_MAX(p.out[2], 1.0f);
      }
    }
    else {
      p.out[2] = 0.0f;
    }

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Glare Operation ******** */

void MixGlareOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];
  float value, input_weight, glare_weight;

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
  value = inputValue[0];
  /* Linear interpolation between 3 cases:
   *  value=-1:output=input    value=0:output=input+glare   value=1:output=glare
   */
  if (value < 0.0f) {
    input_weight = 1.0f;
    glare_weight = 1.0f + value;
  }
  else {
    input_weight = 1.0f - value;
    glare_weight = 1.0f;
  }
  output[0] = input_weight * MAX2(inputColor1[0], 0.0f) + glare_weight * inputColor2[0];
  output[1] = input_weight * MAX2(inputColor1[1], 0.0f) + glare_weight * inputColor2[1];
  output[2] = input_weight * MAX2(inputColor1[2], 0.0f) + glare_weight * inputColor2[2];
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixGlareOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    const float value = p.value[0];
    /* Linear interpolation between 3 cases:
     *  value=-1:output=input    value=0:output=input+glare   value=1:output=glare
     */
    float input_weight;
    float glare_weight;
    if (value < 0.0f) {
      input_weight = 1.0f;
      glare_weight = 1.0f + value;
    }
    else {
      input_weight = 1.0f - value;
      glare_weight = 1.0f;
    }
    p.out[0] = input_weight * MAX2(p.color1[0], 0.0f) + glare_weight * p.color2[0];
    p.out[1] = input_weight * MAX2(p.color1[1], 0.0f) + glare_weight * p.color2[1];
    p.out[2] = input_weight * MAX2(p.color1[2], 0.0f) + glare_weight * p.color2[2];
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Hue Operation ******** */

void MixHueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  float colH, colS, colV;
  rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
  if (colS != 0.0f) {
    float rH, rS, rV;
    float tmpr, tmpg, tmpb;
    rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
    hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
    output[0] = valuem * (inputColor1[0]) + value * tmpr;
    output[1] = valuem * (inputColor1[1]) + value * tmpg;
    output[2] = valuem * (inputColor1[2]) + value * tmpb;
  }
  else {
    copy_v3_v3(output, inputColor1);
  }
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixHueOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    float colH, colS, colV;
    rgb_to_hsv(p.color2[0], p.color2[1], p.color2[2], &colH, &colS, &colV);
    if (colS != 0.0f) {
      float rH, rS, rV;
      float tmpr, tmpg, tmpb;
      rgb_to_hsv(p.color1[0], p.color1[1], p.color1[2], &rH, &rS, &rV);
      hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
      p.out[0] = value_m * p.color1[0] + value * tmpr;
      p.out[1] = value_m * p.color1[1] + value * tmpg;
      p.out[2] = value_m * p.color1[2] + value * tmpb;
    }
    else {
      copy_v3_v3(p.out, p.color1);
    }
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Lighten Operation ******** */

void MixLightenOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float tmp;
  tmp = value * inputColor2[0];
  if (tmp > inputColor1[0]) {
    output[0] = tmp;
  }
  else {
    output[0] = inputColor1[0];
  }
  tmp = value * inputColor2[1];
  if (tmp > inputColor1[1]) {
    output[1] = tmp;
  }
  else {
    output[1] = inputColor1[1];
  }
  tmp = value * inputColor2[2];
  if (tmp > inputColor1[2]) {
    output[2] = tmp;
  }
  else {
    output[2] = inputColor1[2];
  }
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixLightenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }

    float tmp = value * p.color2[0];
    p.out[0] = MAX2(tmp, p.color1[0]);

    tmp = value * p.color2[1];
    p.out[1] = MAX2(tmp, p.color1[1]);

    tmp = value * p.color2[2];
    p.out[2] = MAX2(tmp, p.color1[2]);

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Linear Light Operation ******** */

void MixLinearLightOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  if (inputColor2[0] > 0.5f) {
    output[0] = inputColor1[0] + value * (2.0f * (inputColor2[0] - 0.5f));
  }
  else {
    output[0] = inputColor1[0] + value * (2.0f * (inputColor2[0]) - 1.0f);
  }
  if (inputColor2[1] > 0.5f) {
    output[1] = inputColor1[1] + value * (2.0f * (inputColor2[1] - 0.5f));
  }
  else {
    output[1] = inputColor1[1] + value * (2.0f * (inputColor2[1]) - 1.0f);
  }
  if (inputColor2[2] > 0.5f) {
    output[2] = inputColor1[2] + value * (2.0f * (inputColor2[2] - 0.5f));
  }
  else {
    output[2] = inputColor1[2] + value * (2.0f * (inputColor2[2]) - 1.0f);
  }

  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixLinearLightOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    if (p.color2[0] > 0.5f) {
      p.out[0] = p.color1[0] + value * (2.0f * (p.color2[0] - 0.5f));
    }
    else {
      p.out[0] = p.color1[0] + value * (2.0f * (p.color2[0]) - 1.0f);
    }
    if (p.color2[1] > 0.5f) {
      p.out[1] = p.color1[1] + value * (2.0f * (p.color2[1] - 0.5f));
    }
    else {
      p.out[1] = p.color1[1] + value * (2.0f * (p.color2[1]) - 1.0f);
    }
    if (p.color2[2] > 0.5f) {
      p.out[2] = p.color1[2] + value * (2.0f * (p.color2[2] - 0.5f));
    }
    else {
      p.out[2] = p.color1[2] + value * (2.0f * (p.color2[2]) - 1.0f);
    }

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Multiply Operation ******** */

void MixMultiplyOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  output[0] = inputColor1[0] * (valuem + value * inputColor2[0]);
  output[1] = inputColor1[1] * (valuem + value * inputColor2[1]);
  output[2] = inputColor1[2] * (valuem + value * inputColor2[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixMultiplyOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = p.color1[0] * (value_m + value * p.color2[0]);
    p.out[1] = p.color1[1] * (value_m + value * p.color2[1]);
    p.out[2] = p.color1[2] * (value_m + value * p.color2[2]);

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Overlay Operation ******** */

void MixOverlayOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }

  float valuem = 1.0f - value;

  if (inputColor1[0] < 0.5f) {
    output[0] = inputColor1[0] * (valuem + 2.0f * value * inputColor2[0]);
  }
  else {
    output[0] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[0])) * (1.0f - inputColor1[0]);
  }
  if (inputColor1[1] < 0.5f) {
    output[1] = inputColor1[1] * (valuem + 2.0f * value * inputColor2[1]);
  }
  else {
    output[1] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[1])) * (1.0f - inputColor1[1]);
  }
  if (inputColor1[2] < 0.5f) {
    output[2] = inputColor1[2] * (valuem + 2.0f * value * inputColor2[2]);
  }
  else {
    output[2] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[2])) * (1.0f - inputColor1[2]);
  }
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixOverlayOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    if (p.color1[0] < 0.5f) {
      p.out[0] = p.color1[0] * (value_m + 2.0f * value * p.color2[0]);
    }
    else {
      p.out[0] = 1.0f - (value_m + 2.0f * value * (1.0f - p.color2[0])) * (1.0f - p.color1[0]);
    }
    if (p.color1[1] < 0.5f) {
      p.out[1] = p.color1[1] * (value_m + 2.0f * value * p.color2[1]);
    }
    else {
      p.out[1] = 1.0f - (value_m + 2.0f * value * (1.0f - p.color2[1])) * (1.0f - p.color1[1]);
    }
    if (p.color1[2] < 0.5f) {
      p.out[2] = p.color1[2] * (value_m + 2.0f * value * p.color2[2]);
    }
    else {
      p.out[2] = 1.0f - (value_m + 2.0f * value * (1.0f - p.color2[2])) * (1.0f - p.color1[2]);
    }

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Saturation Operation ******** */

void MixSaturationOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  float rH, rS, rV;
  rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
  if (rS != 0.0f) {
    float colH, colS, colV;
    rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
    hsv_to_rgb(rH, (valuem * rS + value * colS), rV, &output[0], &output[1], &output[2]);
  }
  else {
    copy_v3_v3(output, inputColor1);
  }

  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixSaturationOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    float rH, rS, rV;
    rgb_to_hsv(p.color1[0], p.color1[1], p.color1[2], &rH, &rS, &rV);
    if (rS != 0.0f) {
      float colH, colS, colV;
      rgb_to_hsv(p.color2[0], p.color2[1], p.color2[2], &colH, &colS, &colV);
      hsv_to_rgb(rH, (value_m * rS + value * colS), rV, &p.out[0], &p.out[1], &p.out[2]);
    }
    else {
      copy_v3_v3(p.out, p.color1);
    }

    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Screen Operation ******** */

void MixScreenOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  output[0] = 1.0f - (valuem + value * (1.0f - inputColor2[0])) * (1.0f - inputColor1[0]);
  output[1] = 1.0f - (valuem + value * (1.0f - inputColor2[1])) * (1.0f - inputColor1[1]);
  output[2] = 1.0f - (valuem + value * (1.0f - inputColor2[2])) * (1.0f - inputColor1[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixScreenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    p.out[0] = 1.0f - (value_m + value * (1.0f - p.color2[0])) * (1.0f - p.color1[0]);
    p.out[1] = 1.0f - (value_m + value * (1.0f - p.color2[1])) * (1.0f - p.color1[1]);
    p.out[2] = 1.0f - (value_m + value * (1.0f - p.color2[2])) * (1.0f - p.color1[2]);
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Soft Light Operation ******** */

void MixSoftLightOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;
  float scr, scg, scb;

  /* first calculate non-fac based Screen mix */
  scr = 1.0f - (1.0f - inputColor2[0]) * (1.0f - inputColor1[0]);
  scg = 1.0f - (1.0f - inputColor2[1]) * (1.0f - inputColor1[1]);
  scb = 1.0f - (1.0f - inputColor2[2]) * (1.0f - inputColor1[2]);

  output[0] = valuem * (inputColor1[0]) +
              value * (((1.0f - inputColor1[0]) * inputColor2[0] * (inputColor1[0])) +
                       (inputColor1[0] * scr));
  output[1] = valuem * (inputColor1[1]) +
              value * (((1.0f - inputColor1[1]) * inputColor2[1] * (inputColor1[1])) +
                       (inputColor1[1] * scg));
  output[2] = valuem * (inputColor1[2]) +
              value * (((1.0f - inputColor1[2]) * inputColor2[2] * (inputColor1[2])) +
                       (inputColor1[2] * scb));
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixSoftLightOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    float scr, scg, scb;

    /* First calculate non-fac based Screen mix. */
    scr = 1.0f - (1.0f - p.color2[0]) * (1.0f - p.color1[0]);
    scg = 1.0f - (1.0f - p.color2[1]) * (1.0f - p.color1[1]);
    scb = 1.0f - (1.0f - p.color2[2]) * (1.0f - p.color1[2]);

    p.out[0] = value_m * p.color1[0] +
               value * ((1.0f - p.color1[0]) * p.color2[0] * p.color1[0] + p.color1[0] * scr);
    p.out[1] = value_m * p.color1[1] +
               value * ((1.0f - p.color1[1]) * p.color2[1] * p.color1[1] + p.color1[1] * scg);
    p.out[2] = value_m * p.color1[2] +
               value * ((1.0f - p.color1[2]) * p.color2[2] * p.color1[2] + p.color1[2] * scb);
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Subtract Operation ******** */

void MixSubtractOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  output[0] = inputColor1[0] - value * (inputColor2[0]);
  output[1] = inputColor1[1] - value * (inputColor2[1]);
  output[2] = inputColor1[2] - value * (inputColor2[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixSubtractOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    p.out[0] = p.color1[0] - value * p.color2[0];
    p.out[1] = p.color1[1] - value * p.color2[1];
    p.out[2] = p.color1[2] - value * p.color2[2];
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

/* ******** Mix Value Operation ******** */

void MixValueOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputColor1[4];
  float inputColor2[4];
  float inputValue[4];

  this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
  this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
  this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

  float value = inputValue[0];
  if (this->useValueAlphaMultiply()) {
    value *= inputColor2[3];
  }
  float valuem = 1.0f - value;

  float rH, rS, rV;
  float colH, colS, colV;
  rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
  rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
  hsv_to_rgb(rH, rS, (valuem * rV + value * colV), &output[0], &output[1], &output[2]);
  output[3] = inputColor1[3];

  clampIfNeeded(output);
}

void MixValueOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->useValueAlphaMultiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;

    float rH, rS, rV;
    float colH, colS, colV;
    rgb_to_hsv(p.color1[0], p.color1[1], p.color1[2], &rH, &rS, &rV);
    rgb_to_hsv(p.color2[0], p.color2[1], p.color2[2], &colH, &colS, &colV);
    hsv_to_rgb(rH, rS, (value_m * rV + value * colV), &p.out[0], &p.out[1], &p.out[2]);
    p.out[3] = p.color1[3];

    clampIfNeeded(p.out);
    p.next();
  }
}

}  // namespace blender::compositor
