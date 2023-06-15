/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorBalanceLGGOperation.h"

namespace blender::compositor {

inline float colorbalance_lgg(float in, float lift_lgg, float gamma_inv, float gain)
{
  /* 1:1 match with the sequencer with linear/srgb conversions, the conversion isn't pretty
   * but best keep it this way, since testing for durian shows a similar calculation
   * without lin/srgb conversions gives bad results (over-saturated shadows) with colors
   * slightly below 1.0. some correction can be done but it ends up looking bad for shadows or
   * lighter tones - campbell */
  float x = (((linearrgb_to_srgb(in) - 1.0f) * lift_lgg) + 1.0f) * gain;

  /* prevent NaN */
  if (x < 0.0f) {
    x = 0.0f;
  }

  return powf(srgb_to_linearrgb(x), gamma_inv);
}

ColorBalanceLGGOperation::ColorBalanceLGGOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_value_operation_ = nullptr;
  input_color_operation_ = nullptr;
  this->set_canvas_input_index(1);
  flags_.can_be_constant = true;
}

void ColorBalanceLGGOperation::init_execution()
{
  input_value_operation_ = this->get_input_socket_reader(0);
  input_color_operation_ = this->get_input_socket_reader(1);
}

void ColorBalanceLGGOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float input_color[4];
  float value[4];

  input_value_operation_->read_sampled(value, x, y, sampler);
  input_color_operation_->read_sampled(input_color, x, y, sampler);

  float fac = value[0];
  fac = MIN2(1.0f, fac);
  const float mfac = 1.0f - fac;

  output[0] = mfac * input_color[0] +
              fac * colorbalance_lgg(input_color[0], lift_[0], gamma_inv_[0], gain_[0]);
  output[1] = mfac * input_color[1] +
              fac * colorbalance_lgg(input_color[1], lift_[1], gamma_inv_[1], gain_[1]);
  output[2] = mfac * input_color[2] +
              fac * colorbalance_lgg(input_color[2], lift_[2], gamma_inv_[2], gain_[2]);
  output[3] = input_color[3];
}

void ColorBalanceLGGOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_factor = p.ins[0];
    const float *in_color = p.ins[1];
    const float fac = MIN2(1.0f, in_factor[0]);
    const float fac_m = 1.0f - fac;
    p.out[0] = fac_m * in_color[0] +
               fac * colorbalance_lgg(in_color[0], lift_[0], gamma_inv_[0], gain_[0]);
    p.out[1] = fac_m * in_color[1] +
               fac * colorbalance_lgg(in_color[1], lift_[1], gamma_inv_[1], gain_[1]);
    p.out[2] = fac_m * in_color[2] +
               fac * colorbalance_lgg(in_color[2], lift_[2], gamma_inv_[2], gain_[2]);
    p.out[3] = in_color[3];
  }
}

void ColorBalanceLGGOperation::deinit_execution()
{
  input_value_operation_ = nullptr;
  input_color_operation_ = nullptr;
}

}  // namespace blender::compositor
