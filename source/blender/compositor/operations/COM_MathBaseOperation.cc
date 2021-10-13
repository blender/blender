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

#include "COM_MathBaseOperation.h"

namespace blender::compositor {

MathBaseOperation::MathBaseOperation()
{
  /* TODO(manzanilla): after removing tiled implementation, template this class to only add needed
   * number of inputs. */
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  m_inputValue1Operation = nullptr;
  m_inputValue2Operation = nullptr;
  m_inputValue3Operation = nullptr;
  m_useClamp = false;
  this->flags.can_be_constant = true;
}

void MathBaseOperation::initExecution()
{
  m_inputValue1Operation = this->getInputSocketReader(0);
  m_inputValue2Operation = this->getInputSocketReader(1);
  m_inputValue3Operation = this->getInputSocketReader(2);
}

void MathBaseOperation::deinitExecution()
{
  m_inputValue1Operation = nullptr;
  m_inputValue2Operation = nullptr;
  m_inputValue3Operation = nullptr;
}

void MathBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperationInput *socket;
  rcti temp_area;
  socket = this->getInputSocket(0);
  const bool determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
  if (determined) {
    this->set_canvas_input_index(0);
  }
  else {
    this->set_canvas_input_index(1);
  }
  NodeOperation::determine_canvas(preferred_area, r_area);
}

void MathBaseOperation::clampIfNeeded(float *color)
{
  if (m_useClamp) {
    CLAMP(color[0], 0.0f, 1.0f);
  }
}

void MathBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  BuffersIterator<float> it = output->iterate_with(inputs, area);
  update_memory_buffer_partial(it);
}

void MathAddOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = inputValue1[0] + inputValue2[0];

  clampIfNeeded(output);
}

void MathSubtractOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = inputValue1[0] - inputValue2[0];

  clampIfNeeded(output);
}

void MathMultiplyOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = inputValue1[0] * inputValue2[0];

  clampIfNeeded(output);
}

void MathDivideOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue2[0] == 0) { /* We don't want to divide by zero. */
    output[0] = 0.0;
  }
  else {
    output[0] = inputValue1[0] / inputValue2[0];
  }

  clampIfNeeded(output);
}

void MathDivideOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float divisor = *it.in(1);
    *it.out = clamp_when_enabled((divisor == 0) ? 0 : *it.in(0) / divisor);
  }
}

void MathSineOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = sin(inputValue1[0]);

  clampIfNeeded(output);
}

void MathSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sin(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCosineOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = cos(inputValue1[0]);

  clampIfNeeded(output);
}

void MathCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cos(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTangentOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = tan(inputValue1[0]);

  clampIfNeeded(output);
}

void MathTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicSineOperation::executePixelSampled(float output[4],
                                                      float x,
                                                      float y,
                                                      PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = sinh(inputValue1[0]);

  clampIfNeeded(output);
}

void MathHyperbolicSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sinh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicCosineOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = cosh(inputValue1[0]);

  clampIfNeeded(output);
}

void MathHyperbolicCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cosh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicTangentOperation::executePixelSampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = tanh(inputValue1[0]);

  clampIfNeeded(output);
}

void MathHyperbolicTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tanh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcSineOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue1[0] <= 1 && inputValue1[0] >= -1) {
    output[0] = asin(inputValue1[0]);
  }
  else {
    output[0] = 0.0;
  }

  clampIfNeeded(output);
}

void MathArcSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? asin(value1) : 0.0f);
  }
}

void MathArcCosineOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue1[0] <= 1 && inputValue1[0] >= -1) {
    output[0] = acos(inputValue1[0]);
  }
  else {
    output[0] = 0.0;
  }

  clampIfNeeded(output);
}

void MathArcCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? acos(value1) : 0.0f);
  }
}

void MathArcTangentOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = atan(inputValue1[0]);

  clampIfNeeded(output);
}

void MathArcTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathPowerOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue1[0] >= 0) {
    output[0] = pow(inputValue1[0], inputValue2[0]);
  }
  else {
    float y_mod_1 = fmod(inputValue2[0], 1);
    /* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
    if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
      output[0] = pow(inputValue1[0], floorf(inputValue2[0] + 0.5f));
    }
    else {
      output[0] = 0.0;
    }
  }

  clampIfNeeded(output);
}

void MathPowerOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 >= 0) {
      *it.out = pow(value1, value2);
    }
    else {
      const float y_mod_1 = fmod(value2, 1);
      /* If input value is not nearly an integer, fall back to zero, nicer than straight rounding.
       */
      if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
        *it.out = pow(value1, floorf(value2 + 0.5f));
      }
      else {
        *it.out = 0.0f;
      }
    }
    clamp_when_enabled(it.out);
  }
}

void MathLogarithmOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue1[0] > 0 && inputValue2[0] > 0) {
    output[0] = log(inputValue1[0]) / log(inputValue2[0]);
  }
  else {
    output[0] = 0.0;
  }

  clampIfNeeded(output);
}

void MathLogarithmOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 > 0 && value2 > 0) {
      *it.out = log(value1) / log(value2);
    }
    else {
      *it.out = 0.0;
    }
    clamp_when_enabled(it.out);
  }
}

void MathMinimumOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = MIN2(inputValue1[0], inputValue2[0]);

  clampIfNeeded(output);
}

void MathMinimumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = MIN2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathMaximumOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = MAX2(inputValue1[0], inputValue2[0]);

  clampIfNeeded(output);
}

void MathMaximumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = MAX2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathRoundOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = round(inputValue1[0]);

  clampIfNeeded(output);
}

void MathRoundOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = round(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathLessThanOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = inputValue1[0] < inputValue2[0] ? 1.0f : 0.0f;

  clampIfNeeded(output);
}

void MathGreaterThanOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = inputValue1[0] > inputValue2[0] ? 1.0f : 0.0f;

  clampIfNeeded(output);
}

void MathModuloOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue2[0] == 0) {
    output[0] = 0.0;
  }
  else {
    output[0] = fmod(inputValue1[0], inputValue2[0]);
  }

  clampIfNeeded(output);
}

void MathModuloOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value2 = *it.in(1);
    *it.out = (value2 == 0) ? 0 : fmod(*it.in(0), value2);
    clamp_when_enabled(it.out);
  }
}

void MathAbsoluteOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = fabs(inputValue1[0]);

  clampIfNeeded(output);
}

void MathAbsoluteOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = fabs(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathRadiansOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = DEG2RADF(inputValue1[0]);

  clampIfNeeded(output);
}

void MathRadiansOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = DEG2RADF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathDegreesOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = RAD2DEGF(inputValue1[0]);

  clampIfNeeded(output);
}

void MathDegreesOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = RAD2DEGF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcTan2Operation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = atan2(inputValue1[0], inputValue2[0]);

  clampIfNeeded(output);
}

void MathArcTan2Operation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathFloorOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = floor(inputValue1[0]);

  clampIfNeeded(output);
}

void MathFloorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = floor(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCeilOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = ceil(inputValue1[0]);

  clampIfNeeded(output);
}

void MathCeilOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = ceil(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathFractOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = inputValue1[0] - floor(inputValue1[0]);

  clampIfNeeded(output);
}

void MathFractOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value - floor(value));
  }
}

void MathSqrtOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  if (inputValue1[0] > 0) {
    output[0] = sqrt(inputValue1[0]);
  }
  else {
    output[0] = 0.0f;
  }

  clampIfNeeded(output);
}

void MathSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? sqrt(value) : 0.0f);
  }
}

void MathInverseSqrtOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  if (inputValue1[0] > 0) {
    output[0] = 1.0f / sqrt(inputValue1[0]);
  }
  else {
    output[0] = 0.0f;
  }

  clampIfNeeded(output);
}

void MathInverseSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? 1.0f / sqrt(value) : 0.0f);
  }
}

void MathSignOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = compatible_signf(inputValue1[0]);

  clampIfNeeded(output);
}

void MathSignOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = compatible_signf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathExponentOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = expf(inputValue1[0]);

  clampIfNeeded(output);
}

void MathExponentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = expf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTruncOperation::executePixelSampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float inputValue1[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);

  output[0] = (inputValue1[0] >= 0.0f) ? floor(inputValue1[0]) : ceil(inputValue1[0]);

  clampIfNeeded(output);
}

void MathTruncOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = (value >= 0.0f) ? floor(value) : ceil(value);
    clamp_when_enabled(it.out);
  }
}

void MathSnapOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  if (inputValue1[0] == 0 || inputValue2[0] == 0) { /* We don't want to divide by zero. */
    output[0] = 0.0f;
  }
  else {
    output[0] = floorf(inputValue1[0] / inputValue2[0]) * inputValue2[0];
  }

  clampIfNeeded(output);
}

void MathSnapOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value1 = *it.in(0);
    const float value2 = *it.in(1);
    if (value1 == 0 || value2 == 0) { /* Avoid dividing by zero. */
      *it.out = 0.0f;
    }
    else {
      *it.out = floorf(value1 / value2) * value2;
    }
    clamp_when_enabled(it.out);
  }
}

void MathWrapOperation::executePixelSampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];
  float inputValue3[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
  m_inputValue3Operation->readSampled(inputValue3, x, y, sampler);

  output[0] = wrapf(inputValue1[0], inputValue2[0], inputValue3[0]);

  clampIfNeeded(output);
}

void MathWrapOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = wrapf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathPingpongOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);

  output[0] = pingpongf(inputValue1[0], inputValue2[0]);

  clampIfNeeded(output);
}

void MathPingpongOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = pingpongf(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathCompareOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];
  float inputValue3[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
  m_inputValue3Operation->readSampled(inputValue3, x, y, sampler);

  output[0] = (fabsf(inputValue1[0] - inputValue2[0]) <= MAX2(inputValue3[0], 1e-5f)) ? 1.0f :
                                                                                        0.0f;

  clampIfNeeded(output);
}

void MathCompareOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = (fabsf(*it.in(0) - *it.in(1)) <= MAX2(*it.in(2), 1e-5f)) ? 1.0f : 0.0f;
    clamp_when_enabled(it.out);
  }
}

void MathMultiplyAddOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];
  float inputValue3[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
  m_inputValue3Operation->readSampled(inputValue3, x, y, sampler);

  output[0] = inputValue1[0] * inputValue2[0] + inputValue3[0];

  clampIfNeeded(output);
}

void MathMultiplyAddOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = it.in(0)[0] * it.in(1)[0] + it.in(2)[0];
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMinOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];
  float inputValue3[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
  m_inputValue3Operation->readSampled(inputValue3, x, y, sampler);

  output[0] = smoothminf(inputValue1[0], inputValue2[0], inputValue3[0]);

  clampIfNeeded(output);
}

void MathSmoothMinOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = smoothminf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMaxOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float inputValue1[4];
  float inputValue2[4];
  float inputValue3[4];

  m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
  m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
  m_inputValue3Operation->readSampled(inputValue3, x, y, sampler);

  output[0] = -smoothminf(-inputValue1[0], -inputValue2[0], inputValue3[0]);

  clampIfNeeded(output);
}

void MathSmoothMaxOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = -smoothminf(-it.in(0)[0], -it.in(1)[0], it.in(2)[0]);
    clamp_when_enabled(it.out);
  }
}

}  // namespace blender::compositor
