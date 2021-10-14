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
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_value1_operation_ = nullptr;
  input_value2_operation_ = nullptr;
  input_value3_operation_ = nullptr;
  use_clamp_ = false;
  flags_.can_be_constant = true;
}

void MathBaseOperation::init_execution()
{
  input_value1_operation_ = this->get_input_socket_reader(0);
  input_value2_operation_ = this->get_input_socket_reader(1);
  input_value3_operation_ = this->get_input_socket_reader(2);
}

void MathBaseOperation::deinit_execution()
{
  input_value1_operation_ = nullptr;
  input_value2_operation_ = nullptr;
  input_value3_operation_ = nullptr;
}

void MathBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperationInput *socket;
  rcti temp_area;
  socket = this->get_input_socket(0);
  const bool determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
  if (determined) {
    this->set_canvas_input_index(0);
  }
  else {
    this->set_canvas_input_index(1);
  }
  NodeOperation::determine_canvas(preferred_area, r_area);
}

void MathBaseOperation::clamp_if_needed(float *color)
{
  if (use_clamp_) {
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

void MathAddOperation::execute_pixel_sampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = input_value1[0] + input_value2[0];

  clamp_if_needed(output);
}

void MathSubtractOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = input_value1[0] - input_value2[0];

  clamp_if_needed(output);
}

void MathMultiplyOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = input_value1[0] * input_value2[0];

  clamp_if_needed(output);
}

void MathDivideOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value2[0] == 0) { /* We don't want to divide by zero. */
    output[0] = 0.0;
  }
  else {
    output[0] = input_value1[0] / input_value2[0];
  }

  clamp_if_needed(output);
}

void MathDivideOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float divisor = *it.in(1);
    *it.out = clamp_when_enabled((divisor == 0) ? 0 : *it.in(0) / divisor);
  }
}

void MathSineOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = sin(input_value1[0]);

  clamp_if_needed(output);
}

void MathSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sin(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCosineOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = cos(input_value1[0]);

  clamp_if_needed(output);
}

void MathCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cos(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTangentOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = tan(input_value1[0]);

  clamp_if_needed(output);
}

void MathTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicSineOperation::execute_pixel_sampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = sinh(input_value1[0]);

  clamp_if_needed(output);
}

void MathHyperbolicSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = sinh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicCosineOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = cosh(input_value1[0]);

  clamp_if_needed(output);
}

void MathHyperbolicCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = cosh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathHyperbolicTangentOperation::execute_pixel_sampled(float output[4],
                                                           float x,
                                                           float y,
                                                           PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = tanh(input_value1[0]);

  clamp_if_needed(output);
}

void MathHyperbolicTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = tanh(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcSineOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value1[0] <= 1 && input_value1[0] >= -1) {
    output[0] = asin(input_value1[0]);
  }
  else {
    output[0] = 0.0;
  }

  clamp_if_needed(output);
}

void MathArcSineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? asin(value1) : 0.0f);
  }
}

void MathArcCosineOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value1[0] <= 1 && input_value1[0] >= -1) {
    output[0] = acos(input_value1[0]);
  }
  else {
    output[0] = 0.0;
  }

  clamp_if_needed(output);
}

void MathArcCosineOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    float value1 = *it.in(0);
    *it.out = clamp_when_enabled((value1 <= 1 && value1 >= -1) ? acos(value1) : 0.0f);
  }
}

void MathArcTangentOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = atan(input_value1[0]);

  clamp_if_needed(output);
}

void MathArcTangentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathPowerOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value1[0] >= 0) {
    output[0] = pow(input_value1[0], input_value2[0]);
  }
  else {
    float y_mod_1 = fmod(input_value2[0], 1);
    /* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
    if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
      output[0] = pow(input_value1[0], floorf(input_value2[0] + 0.5f));
    }
    else {
      output[0] = 0.0;
    }
  }

  clamp_if_needed(output);
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

void MathLogarithmOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value1[0] > 0 && input_value2[0] > 0) {
    output[0] = log(input_value1[0]) / log(input_value2[0]);
  }
  else {
    output[0] = 0.0;
  }

  clamp_if_needed(output);
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

void MathMinimumOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = MIN2(input_value1[0], input_value2[0]);

  clamp_if_needed(output);
}

void MathMinimumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = MIN2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathMaximumOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = MAX2(input_value1[0], input_value2[0]);

  clamp_if_needed(output);
}

void MathMaximumOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = MAX2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathRoundOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = round(input_value1[0]);

  clamp_if_needed(output);
}

void MathRoundOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = round(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathLessThanOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = input_value1[0] < input_value2[0] ? 1.0f : 0.0f;

  clamp_if_needed(output);
}

void MathGreaterThanOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = input_value1[0] > input_value2[0] ? 1.0f : 0.0f;

  clamp_if_needed(output);
}

void MathModuloOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value2[0] == 0) {
    output[0] = 0.0;
  }
  else {
    output[0] = fmod(input_value1[0], input_value2[0]);
  }

  clamp_if_needed(output);
}

void MathModuloOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value2 = *it.in(1);
    *it.out = (value2 == 0) ? 0 : fmod(*it.in(0), value2);
    clamp_when_enabled(it.out);
  }
}

void MathAbsoluteOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = fabs(input_value1[0]);

  clamp_if_needed(output);
}

void MathAbsoluteOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = fabs(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathRadiansOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = DEG2RADF(input_value1[0]);

  clamp_if_needed(output);
}

void MathRadiansOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = DEG2RADF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathDegreesOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = RAD2DEGF(input_value1[0]);

  clamp_if_needed(output);
}

void MathDegreesOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = RAD2DEGF(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathArcTan2Operation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = atan2(input_value1[0], input_value2[0]);

  clamp_if_needed(output);
}

void MathArcTan2Operation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = atan2(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathFloorOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = floor(input_value1[0]);

  clamp_if_needed(output);
}

void MathFloorOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = floor(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathCeilOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = ceil(input_value1[0]);

  clamp_if_needed(output);
}

void MathCeilOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = ceil(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathFractOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = input_value1[0] - floor(input_value1[0]);

  clamp_if_needed(output);
}

void MathFractOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value - floor(value));
  }
}

void MathSqrtOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  if (input_value1[0] > 0) {
    output[0] = sqrt(input_value1[0]);
  }
  else {
    output[0] = 0.0f;
  }

  clamp_if_needed(output);
}

void MathSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? sqrt(value) : 0.0f);
  }
}

void MathInverseSqrtOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  if (input_value1[0] > 0) {
    output[0] = 1.0f / sqrt(input_value1[0]);
  }
  else {
    output[0] = 0.0f;
  }

  clamp_if_needed(output);
}

void MathInverseSqrtOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = clamp_when_enabled(value > 0 ? 1.0f / sqrt(value) : 0.0f);
  }
}

void MathSignOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = compatible_signf(input_value1[0]);

  clamp_if_needed(output);
}

void MathSignOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = compatible_signf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathExponentOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = expf(input_value1[0]);

  clamp_if_needed(output);
}

void MathExponentOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = expf(*it.in(0));
    clamp_when_enabled(it.out);
  }
}

void MathTruncOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value1[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);

  output[0] = (input_value1[0] >= 0.0f) ? floor(input_value1[0]) : ceil(input_value1[0]);

  clamp_if_needed(output);
}

void MathTruncOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    const float value = *it.in(0);
    *it.out = (value >= 0.0f) ? floor(value) : ceil(value);
    clamp_when_enabled(it.out);
  }
}

void MathSnapOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  if (input_value1[0] == 0 || input_value2[0] == 0) { /* We don't want to divide by zero. */
    output[0] = 0.0f;
  }
  else {
    output[0] = floorf(input_value1[0] / input_value2[0]) * input_value2[0];
  }

  clamp_if_needed(output);
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

void MathWrapOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];
  float input_value3[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);
  input_value3_operation_->read_sampled(input_value3, x, y, sampler);

  output[0] = wrapf(input_value1[0], input_value2[0], input_value3[0]);

  clamp_if_needed(output);
}

void MathWrapOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = wrapf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathPingpongOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);

  output[0] = pingpongf(input_value1[0], input_value2[0]);

  clamp_if_needed(output);
}

void MathPingpongOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = pingpongf(*it.in(0), *it.in(1));
    clamp_when_enabled(it.out);
  }
}

void MathCompareOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];
  float input_value3[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);
  input_value3_operation_->read_sampled(input_value3, x, y, sampler);

  output[0] = (fabsf(input_value1[0] - input_value2[0]) <= MAX2(input_value3[0], 1e-5f)) ? 1.0f :
                                                                                           0.0f;

  clamp_if_needed(output);
}

void MathCompareOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = (fabsf(*it.in(0) - *it.in(1)) <= MAX2(*it.in(2), 1e-5f)) ? 1.0f : 0.0f;
    clamp_when_enabled(it.out);
  }
}

void MathMultiplyAddOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];
  float input_value3[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);
  input_value3_operation_->read_sampled(input_value3, x, y, sampler);

  output[0] = input_value1[0] * input_value2[0] + input_value3[0];

  clamp_if_needed(output);
}

void MathMultiplyAddOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = it.in(0)[0] * it.in(1)[0] + it.in(2)[0];
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMinOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];
  float input_value3[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);
  input_value3_operation_->read_sampled(input_value3, x, y, sampler);

  output[0] = smoothminf(input_value1[0], input_value2[0], input_value3[0]);

  clamp_if_needed(output);
}

void MathSmoothMinOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = smoothminf(*it.in(0), *it.in(1), *it.in(2));
    clamp_when_enabled(it.out);
  }
}

void MathSmoothMaxOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_value1[4];
  float input_value2[4];
  float input_value3[4];

  input_value1_operation_->read_sampled(input_value1, x, y, sampler);
  input_value2_operation_->read_sampled(input_value2, x, y, sampler);
  input_value3_operation_->read_sampled(input_value3, x, y, sampler);

  output[0] = -smoothminf(-input_value1[0], -input_value2[0], input_value3[0]);

  clamp_if_needed(output);
}

void MathSmoothMaxOperation::update_memory_buffer_partial(BuffersIterator<float> &it)
{
  for (; !it.is_end(); ++it) {
    *it.out = -smoothminf(-it.in(0)[0], -it.in(1)[0], it.in(2)[0]);
    clamp_when_enabled(it.out);
  }
}

}  // namespace blender::compositor
