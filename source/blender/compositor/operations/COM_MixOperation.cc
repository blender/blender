/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MixOperation.h"

#include "BLI_math_color.h"

namespace blender::compositor {

/* ******** Mix Base Operation ******** */

MixBaseOperation::MixBaseOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_value_operation_ = nullptr;
  input_color1_operation_ = nullptr;
  input_color2_operation_ = nullptr;
  this->set_use_value_alpha_multiply(false);
  this->set_use_clamp(false);
  flags_.can_be_constant = true;
}

void MixBaseOperation::init_execution()
{
  input_value_operation_ = this->get_input_socket_reader(0);
  input_color1_operation_ = this->get_input_socket_reader(1);
  input_color2_operation_ = this->get_input_socket_reader(2);
}

void MixBaseOperation::execute_pixel_sampled(float output[4],
                                             float x,
                                             float y,
                                             PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * (input_color1[0]) + value * (input_color2[0]);
  output[1] = valuem * (input_color1[1]) + value * (input_color2[1]);
  output[2] = valuem * (input_color1[2]) + value * (input_color2[2]);
  output[3] = input_color1[3];
}

void MixBaseOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperationInput *socket;
  rcti temp_area = COM_AREA_NONE;

  socket = this->get_input_socket(1);
  bool determined = socket->determine_canvas(COM_AREA_NONE, temp_area);
  if (determined) {
    this->set_canvas_input_index(1);
  }
  else {
    socket = this->get_input_socket(2);
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

void MixBaseOperation::deinit_execution()
{
  input_value_operation_ = nullptr;
  input_color1_operation_ = nullptr;
  input_color2_operation_ = nullptr;
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
    if (this->use_value_alpha_multiply()) {
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

void MixAddOperation::execute_pixel_sampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  output[0] = input_color1[0] + value * input_color2[0];
  output[1] = input_color1[1] + value * input_color2[1];
  output[2] = input_color1[2] + value * input_color2[2];
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixAddOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    p.out[0] = p.color1[0] + value * p.color2[0];
    p.out[1] = p.color1[1] + value * p.color2[1];
    p.out[2] = p.color1[2] + value * p.color2[2];
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Blend Operation ******** */

void MixBlendOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];
  float value;

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);
  value = input_value[0];

  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * (input_color1[0]) + value * (input_color2[0]);
  output[1] = valuem * (input_color1[1]) + value * (input_color2[1]);
  output[2] = valuem * (input_color1[2]) + value * (input_color2[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixBlendOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;
    p.out[0] = value_m * p.color1[0] + value * p.color2[0];
    p.out[1] = value_m * p.color1[1] + value * p.color2[1];
    p.out[2] = value_m * p.color1[2] + value * p.color2[2];
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Burn Operation ******** */

void MixColorBurnOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];
  float tmp;

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  tmp = valuem + value * input_color2[0];
  if (tmp <= 0.0f) {
    output[0] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - input_color1[0]) / tmp;
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

  tmp = valuem + value * input_color2[1];
  if (tmp <= 0.0f) {
    output[1] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - input_color1[1]) / tmp;
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

  tmp = valuem + value * input_color2[2];
  if (tmp <= 0.0f) {
    output[2] = 0.0f;
  }
  else {
    tmp = 1.0f - (1.0f - input_color1[2]) / tmp;
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

  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixColorBurnOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    float tmp = value_m + value * p.color2[0];
    if (tmp <= 0.0f) {
      p.out[0] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[0]) / tmp;
      p.out[0] = std::clamp(tmp, 0.0f, 1.0f);
    }

    tmp = value_m + value * p.color2[1];
    if (tmp <= 0.0f) {
      p.out[1] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[1]) / tmp;
      p.out[1] = std::clamp(tmp, 0.0f, 1.0f);
    }

    tmp = value_m + value * p.color2[2];
    if (tmp <= 0.0f) {
      p.out[2] = 0.0f;
    }
    else {
      tmp = 1.0f - (1.0f - p.color1[2]) / tmp;
      p.out[2] = std::clamp(tmp, 0.0f, 1.0f);
    }
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Color Operation ******** */

void MixColorOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  float colH, colS, colV;
  rgb_to_hsv(input_color2[0], input_color2[1], input_color2[2], &colH, &colS, &colV);
  if (colS != 0.0f) {
    float rH, rS, rV;
    float tmpr, tmpg, tmpb;
    rgb_to_hsv(input_color1[0], input_color1[1], input_color1[2], &rH, &rS, &rV);
    hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
    output[0] = (valuem * input_color1[0]) + (value * tmpr);
    output[1] = (valuem * input_color1[1]) + (value * tmpg);
    output[2] = (valuem * input_color1[2]) + (value * tmpb);
  }
  else {
    copy_v3_v3(output, input_color1);
  }
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixColorOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Darken Operation ******** */

void MixDarkenOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = min_ff(input_color1[0], input_color2[0]) * value + input_color1[0] * valuem;
  output[1] = min_ff(input_color1[1], input_color2[1]) * value + input_color1[1] * valuem;
  output[2] = min_ff(input_color1[2], input_color2[2]) * value + input_color1[2] * valuem;
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixDarkenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;
    p.out[0] = min_ff(p.color1[0], p.color2[0]) * value + p.color1[0] * value_m;
    p.out[1] = min_ff(p.color1[1], p.color2[1]) * value + p.color1[1] * value_m;
    p.out[2] = min_ff(p.color1[2], p.color2[2]) * value + p.color1[2] * value_m;
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Difference Operation ******** */

void MixDifferenceOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = valuem * input_color1[0] + value * fabsf(input_color1[0] - input_color2[0]);
  output[1] = valuem * input_color1[1] + value * fabsf(input_color1[1] - input_color2[1]);
  output[2] = valuem * input_color1[2] + value * fabsf(input_color1[2] - input_color2[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixDifferenceOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = value_m * p.color1[0] + value * fabsf(p.color1[0] - p.color2[0]);
    p.out[1] = value_m * p.color1[1] + value * fabsf(p.color1[1] - p.color2[1]);
    p.out[2] = value_m * p.color1[2] + value * fabsf(p.color1[2] - p.color2[2]);
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Exclusion Operation ******** */

void MixExclusionOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = max_ff(valuem * input_color1[0] + value * (input_color1[0] + input_color2[0] -
                                                         2.0f * input_color1[0] * input_color2[0]),
                     0.0f);
  output[1] = max_ff(valuem * input_color1[1] + value * (input_color1[1] + input_color2[1] -
                                                         2.0f * input_color1[1] * input_color2[1]),
                     0.0f);
  output[2] = max_ff(valuem * input_color1[2] + value * (input_color1[2] + input_color2[2] -
                                                         2.0f * input_color1[2] * input_color2[2]),
                     0.0f);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixExclusionOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = max_ff(value_m * p.color1[0] +
                          value * (p.color1[0] + p.color2[0] - 2.0f * p.color1[0] * p.color2[0]),
                      0.0f);
    p.out[1] = max_ff(value_m * p.color1[1] +
                          value * (p.color1[1] + p.color2[1] - 2.0f * p.color1[1] * p.color2[1]),
                      0.0f);
    p.out[2] = max_ff(value_m * p.color1[2] +
                          value * (p.color1[2] + p.color2[2] - 2.0f * p.color1[2] * p.color2[2]),
                      0.0f);
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Divide Operation ******** */

void MixDivideOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  if (input_color2[0] != 0.0f) {
    output[0] = valuem * (input_color1[0]) + value * (input_color1[0]) / input_color2[0];
  }
  else {
    output[0] = 0.0f;
  }
  if (input_color2[1] != 0.0f) {
    output[1] = valuem * (input_color1[1]) + value * (input_color1[1]) / input_color2[1];
  }
  else {
    output[1] = 0.0f;
  }
  if (input_color2[2] != 0.0f) {
    output[2] = valuem * (input_color1[2]) + value * (input_color1[2]) / input_color2[2];
  }
  else {
    output[2] = 0.0f;
  }

  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixDivideOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Dodge Operation ******** */

void MixDodgeOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];
  float tmp;

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }

  if (input_color1[0] != 0.0f) {
    tmp = 1.0f - value * input_color2[0];
    if (tmp <= 0.0f) {
      output[0] = 1.0f;
    }
    else {
      tmp = input_color1[0] / tmp;
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

  if (input_color1[1] != 0.0f) {
    tmp = 1.0f - value * input_color2[1];
    if (tmp <= 0.0f) {
      output[1] = 1.0f;
    }
    else {
      tmp = input_color1[1] / tmp;
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

  if (input_color1[2] != 0.0f) {
    tmp = 1.0f - value * input_color2[2];
    if (tmp <= 0.0f) {
      output[2] = 1.0f;
    }
    else {
      tmp = input_color1[2] / tmp;
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

  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixDodgeOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Glare Operation ******** */

void MixGlareOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];
  float value, input_weight, glare_weight;

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);
  value = input_value[0];
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
  output[0] = input_weight * std::max(input_color1[0], 0.0f) + glare_weight * input_color2[0];
  output[1] = input_weight * std::max(input_color1[1], 0.0f) + glare_weight * input_color2[1];
  output[2] = input_weight * std::max(input_color1[2], 0.0f) + glare_weight * input_color2[2];
  output[3] = input_color1[3];

  clamp_if_needed(output);
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
    p.out[0] = input_weight * std::max(p.color1[0], 0.0f) + glare_weight * p.color2[0];
    p.out[1] = input_weight * std::max(p.color1[1], 0.0f) + glare_weight * p.color2[1];
    p.out[2] = input_weight * std::max(p.color1[2], 0.0f) + glare_weight * p.color2[2];
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Hue Operation ******** */

void MixHueOperation::execute_pixel_sampled(float output[4],
                                            float x,
                                            float y,
                                            PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  float colH, colS, colV;
  rgb_to_hsv(input_color2[0], input_color2[1], input_color2[2], &colH, &colS, &colV);
  if (colS != 0.0f) {
    float rH, rS, rV;
    float tmpr, tmpg, tmpb;
    rgb_to_hsv(input_color1[0], input_color1[1], input_color1[2], &rH, &rS, &rV);
    hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
    output[0] = valuem * (input_color1[0]) + value * tmpr;
    output[1] = valuem * (input_color1[1]) + value * tmpg;
    output[2] = valuem * (input_color1[2]) + value * tmpb;
  }
  else {
    copy_v3_v3(output, input_color1);
  }
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixHueOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Lighten Operation ******** */

void MixLightenOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = max_ff(input_color1[0], input_color2[0]) * value + input_color1[0] * valuem;
  output[1] = max_ff(input_color1[1], input_color2[1]) * value + input_color1[1] * valuem;
  output[2] = max_ff(input_color1[2], input_color2[2]) * value + input_color1[2] * valuem;
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixLightenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;
    p.out[0] = max_ff(p.color1[0], p.color2[0]) * value + p.color1[0] * value_m;
    p.out[1] = max_ff(p.color1[1], p.color2[1]) * value + p.color1[1] * value_m;
    p.out[2] = max_ff(p.color1[2], p.color2[2]) * value + p.color1[2] * value_m;
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Linear Light Operation ******** */

void MixLinearLightOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  if (input_color2[0] > 0.5f) {
    output[0] = input_color1[0] + value * (2.0f * (input_color2[0] - 0.5f));
  }
  else {
    output[0] = input_color1[0] + value * (2.0f * (input_color2[0]) - 1.0f);
  }
  if (input_color2[1] > 0.5f) {
    output[1] = input_color1[1] + value * (2.0f * (input_color2[1] - 0.5f));
  }
  else {
    output[1] = input_color1[1] + value * (2.0f * (input_color2[1]) - 1.0f);
  }
  if (input_color2[2] > 0.5f) {
    output[2] = input_color1[2] + value * (2.0f * (input_color2[2] - 0.5f));
  }
  else {
    output[2] = input_color1[2] + value * (2.0f * (input_color2[2]) - 1.0f);
  }

  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixLinearLightOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Multiply Operation ******** */

void MixMultiplyOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  output[0] = input_color1[0] * (valuem + value * input_color2[0]);
  output[1] = input_color1[1] * (valuem + value * input_color2[1]);
  output[2] = input_color1[2] * (valuem + value * input_color2[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixMultiplyOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;
    p.out[0] = p.color1[0] * (value_m + value * p.color2[0]);
    p.out[1] = p.color1[1] * (value_m + value * p.color2[1]);
    p.out[2] = p.color1[2] * (value_m + value * p.color2[2]);

    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Overlay Operation ******** */

void MixOverlayOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }

  float valuem = 1.0f - value;

  if (input_color1[0] < 0.5f) {
    output[0] = input_color1[0] * (valuem + 2.0f * value * input_color2[0]);
  }
  else {
    output[0] = 1.0f -
                (valuem + 2.0f * value * (1.0f - input_color2[0])) * (1.0f - input_color1[0]);
  }
  if (input_color1[1] < 0.5f) {
    output[1] = input_color1[1] * (valuem + 2.0f * value * input_color2[1]);
  }
  else {
    output[1] = 1.0f -
                (valuem + 2.0f * value * (1.0f - input_color2[1])) * (1.0f - input_color1[1]);
  }
  if (input_color1[2] < 0.5f) {
    output[2] = input_color1[2] * (valuem + 2.0f * value * input_color2[2]);
  }
  else {
    output[2] = 1.0f -
                (valuem + 2.0f * value * (1.0f - input_color2[2])) * (1.0f - input_color1[2]);
  }
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixOverlayOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Saturation Operation ******** */

void MixSaturationOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  float rH, rS, rV;
  rgb_to_hsv(input_color1[0], input_color1[1], input_color1[2], &rH, &rS, &rV);
  if (rS != 0.0f) {
    float colH, colS, colV;
    rgb_to_hsv(input_color2[0], input_color2[1], input_color2[2], &colH, &colS, &colV);
    hsv_to_rgb(rH, (valuem * rS + value * colS), rV, &output[0], &output[1], &output[2]);
  }
  else {
    copy_v3_v3(output, input_color1);
  }

  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixSaturationOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Screen Operation ******** */

void MixScreenOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  output[0] = 1.0f - (valuem + value * (1.0f - input_color2[0])) * (1.0f - input_color1[0]);
  output[1] = 1.0f - (valuem + value * (1.0f - input_color2[1])) * (1.0f - input_color1[1]);
  output[2] = 1.0f - (valuem + value * (1.0f - input_color2[2])) * (1.0f - input_color1[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixScreenOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    const float value_m = 1.0f - value;

    p.out[0] = 1.0f - (value_m + value * (1.0f - p.color2[0])) * (1.0f - p.color1[0]);
    p.out[1] = 1.0f - (value_m + value * (1.0f - p.color2[1])) * (1.0f - p.color1[1]);
    p.out[2] = 1.0f - (value_m + value * (1.0f - p.color2[2])) * (1.0f - p.color1[2]);
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Soft Light Operation ******** */

void MixSoftLightOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;
  float scr, scg, scb;

  /* first calculate non-fac based Screen mix */
  scr = 1.0f - (1.0f - input_color2[0]) * (1.0f - input_color1[0]);
  scg = 1.0f - (1.0f - input_color2[1]) * (1.0f - input_color1[1]);
  scb = 1.0f - (1.0f - input_color2[2]) * (1.0f - input_color1[2]);

  output[0] = valuem * (input_color1[0]) +
              value * (((1.0f - input_color1[0]) * input_color2[0] * (input_color1[0])) +
                       (input_color1[0] * scr));
  output[1] = valuem * (input_color1[1]) +
              value * (((1.0f - input_color1[1]) * input_color2[1] * (input_color1[1])) +
                       (input_color1[1] * scg));
  output[2] = valuem * (input_color1[2]) +
              value * (((1.0f - input_color1[2]) * input_color2[2] * (input_color1[2])) +
                       (input_color1[2] * scb));
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixSoftLightOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
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

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Subtract Operation ******** */

void MixSubtractOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  output[0] = input_color1[0] - value * (input_color2[0]);
  output[1] = input_color1[1] - value * (input_color2[1]);
  output[2] = input_color1[2] - value * (input_color2[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixSubtractOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    p.out[0] = p.color1[0] - value * p.color2[0];
    p.out[1] = p.color1[1] - value * p.color2[1];
    p.out[2] = p.color1[2] - value * p.color2[2];
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

/* ******** Mix Value Operation ******** */

void MixValueOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_color1[4];
  float input_color2[4];
  float input_value[4];

  input_value_operation_->read_sampled(input_value, x, y, sampler);
  input_color1_operation_->read_sampled(input_color1, x, y, sampler);
  input_color2_operation_->read_sampled(input_color2, x, y, sampler);

  float value = input_value[0];
  if (this->use_value_alpha_multiply()) {
    value *= input_color2[3];
  }
  float valuem = 1.0f - value;

  float rH, rS, rV;
  float colH, colS, colV;
  rgb_to_hsv(input_color1[0], input_color1[1], input_color1[2], &rH, &rS, &rV);
  rgb_to_hsv(input_color2[0], input_color2[1], input_color2[2], &colH, &colS, &colV);
  hsv_to_rgb(rH, rS, (valuem * rV + value * colV), &output[0], &output[1], &output[2]);
  output[3] = input_color1[3];

  clamp_if_needed(output);
}

void MixValueOperation::update_memory_buffer_row(PixelCursor &p)
{
  while (p.out < p.row_end) {
    float value = p.value[0];
    if (this->use_value_alpha_multiply()) {
      value *= p.color2[3];
    }
    float value_m = 1.0f - value;

    float rH, rS, rV;
    float colH, colS, colV;
    rgb_to_hsv(p.color1[0], p.color1[1], p.color1[2], &rH, &rS, &rV);
    rgb_to_hsv(p.color2[0], p.color2[1], p.color2[2], &colH, &colS, &colV);
    hsv_to_rgb(rH, rS, (value_m * rV + value * colV), &p.out[0], &p.out[1], &p.out[2]);
    p.out[3] = p.color1[3];

    clamp_if_needed(p.out);
    p.next();
  }
}

}  // namespace blender::compositor
