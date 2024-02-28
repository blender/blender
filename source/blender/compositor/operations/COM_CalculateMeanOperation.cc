/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CalculateMeanOperation.h"

#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

CalculateMeanOperation::CalculateMeanOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Value);
  is_calculated_ = false;
  setting_ = 1;
  flags_.is_constant_operation = true;

  needs_canvas_to_get_constant_ = true;
}

void CalculateMeanOperation::init_execution()
{
  is_calculated_ = false;
}

void CalculateMeanOperation::set_setting(int setting)
{
  setting_ = setting;
  switch (setting) {
    case 1: {
      setting_func_ = IMB_colormanagement_get_luminance;
      break;
    }
    case 2: {
      setting_func_ = [](const float *elem) { return elem[0]; };
      break;
    }
    case 3: {
      setting_func_ = [](const float *elem) { return elem[1]; };
      break;
    }
    case 4: {
      setting_func_ = [](const float *elem) { return elem[2]; };
      break;
    }
    case 5: {
      setting_func_ = [](const float *elem) {
        float yuv[3];
        rgb_to_yuv(elem[0], elem[1], elem[2], &yuv[0], &yuv[1], &yuv[2], BLI_YUV_ITU_BT709);
        return yuv[0];
      };
      break;
    }
  }
}

void CalculateMeanOperation::get_area_of_interest(int input_idx,
                                                  const rcti & /*output_area*/,
                                                  rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  r_input_area = get_input_operation(input_idx)->get_canvas();
}

void CalculateMeanOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  ConstantOperation::determine_canvas(preferred_area, r_area);
  r_area = preferred_area;
}

const float *CalculateMeanOperation::get_constant_elem()
{
  /* Node de-duplication uses the constant value as part of a hash for constant operations.
   * The constant is not known in advance here, but need to return something. The value does
   * not really matter, because if two CalculateMean operations are connected to different
   * inputs it will be handled via hash of the input subtree. */
  static float f = 0;
  return &f;
}

void CalculateMeanOperation::update_memory_buffer(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> inputs)
{
  if (!is_calculated_) {
    MemoryBuffer *input = inputs[0];
    constant_value_ = calculate_value(input);
    is_calculated_ = true;
  }

  output->fill(area, &constant_value_);
}

float CalculateMeanOperation::calculate_value(const MemoryBuffer *input) const
{
  return calculate_mean(input);
}

float CalculateMeanOperation::calculate_mean(const MemoryBuffer *input) const
{
  PixelsSum total = {0};
  exec_system_->execute_work<PixelsSum>(
      input->get_rect(),
      [=](const rcti &split) { return calc_area_sum(input, split); },
      total,
      [](PixelsSum &join, const PixelsSum &chunk) {
        join.sum += chunk.sum;
        join.num_pixels += chunk.num_pixels;
      });
  return total.num_pixels == 0 ? 0.0f : total.sum / total.num_pixels;
}

using PixelsSum = CalculateMeanOperation::PixelsSum;
PixelsSum CalculateMeanOperation::calc_area_sum(const MemoryBuffer *input, const rcti &area) const
{
  PixelsSum result = {0};
  for (const float *elem : input->get_buffer_area(area)) {
    if (elem[3] <= 0.0f) {
      continue;
    }
    result.sum += setting_func_(elem);
    result.num_pixels++;
  }
  return result;
}

}  // namespace blender::compositor
