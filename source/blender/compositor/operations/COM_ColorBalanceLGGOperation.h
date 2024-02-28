/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ColorBalanceLGGOperation : public MultiThreadedRowOperation {
 protected:
  float gain_[3];
  float lift_[3];
  float gamma_inv_[3];

 public:
  ColorBalanceLGGOperation();

  void set_gain(const float gain[3])
  {
    copy_v3_v3(gain_, gain);
  }
  void set_lift(const float lift[3])
  {
    copy_v3_v3(lift_, lift);
  }
  void set_gamma_inv(const float gamma_inv[3])
  {
    copy_v3_v3(gamma_inv_, gamma_inv);
  }

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
