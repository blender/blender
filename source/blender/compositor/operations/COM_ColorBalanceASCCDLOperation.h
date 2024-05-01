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
class ColorBalanceASCCDLOperation : public MultiThreadedRowOperation {
 protected:
  float offset_[3];
  float power_[3];
  float slope_[3];

 public:
  ColorBalanceASCCDLOperation();

  void set_offset(float offset[3])
  {
    copy_v3_v3(offset_, offset);
  }
  void set_power(float power[3])
  {
    copy_v3_v3(power_, power);
  }
  void set_slope(float slope[3])
  {
    copy_v3_v3(slope_, slope);
  }

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
