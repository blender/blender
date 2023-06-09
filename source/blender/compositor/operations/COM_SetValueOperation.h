/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConstantOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class SetValueOperation : public ConstantOperation {
 private:
  float value_;

 public:
  /**
   * Default constructor
   */
  SetValueOperation();

  const float *get_constant_elem() override
  {
    return &value_;
  }

  float get_value()
  {
    return value_;
  }
  void set_value(float value)
  {
    value_ = value;
  }

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
};

}  // namespace blender::compositor
