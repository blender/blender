/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_color.hh"
#include "BLI_math_matrix.h"

#include "IMB_colormanagement.hh"

#include "COM_ColorBalanceWhitepointOperation.h"

namespace blender::compositor {

ColorBalanceWhitepointOperation::ColorBalanceWhitepointOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(1);
  flags_.can_be_constant = true;
}

void ColorBalanceWhitepointOperation::init_execution()
{
  float3x3 scene_to_xyz = IMB_colormanagement_get_scene_linear_to_xyz();
  float3x3 xyz_to_scene = IMB_colormanagement_get_xyz_to_scene_linear();
  float3 input = blender::math::whitepoint_from_temp_tint(input_temperature_, input_tint_);
  float3 output = blender::math::whitepoint_from_temp_tint(output_temperature_, output_tint_);
  float3x3 adaption = blender::math::chromatic_adaption_matrix(input, output);
  matrix_ = float4x4(xyz_to_scene * adaption * scene_to_xyz);
}

void ColorBalanceWhitepointOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_factor = p.ins[0];
    const float *in_color = p.ins[1];
    const float fac = std::min(1.0f, in_factor[0]);
    mul_v4_m4v4(p.out, matrix_.ptr(), in_color);
    interp_v4_v4v4(p.out, in_color, p.out, fac);
  }
}

}  // namespace blender::compositor
