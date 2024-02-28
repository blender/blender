/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
};

}  // namespace blender::compositor
