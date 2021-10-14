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

#pragma once

#include "COM_ConstantOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class SetColorOperation : public ConstantOperation {
 private:
  float color_[4];

 public:
  /**
   * Default constructor
   */
  SetColorOperation();

  const float *get_constant_elem() override
  {
    return color_;
  }

  float get_channel1()
  {
    return color_[0];
  }
  void set_channel1(float value)
  {
    color_[0] = value;
  }
  float get_channel2()
  {
    return color_[1];
  }
  void set_channel2(float value)
  {
    color_[1] = value;
  }
  float get_channel3()
  {
    return color_[2];
  }
  void set_channel3(float value)
  {
    color_[2] = value;
  }
  float get_channel4()
  {
    return color_[3];
  }
  void set_channel4(const float value)
  {
    color_[3] = value;
  }
  void set_channels(const float value[4])
  {
    copy_v4_v4(color_, value);
  }

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
};

}  // namespace blender::compositor
