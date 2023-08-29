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
  /**
   * Prefetched reference to the input_program
   */
  SocketReader *input_value_operation_;
  SocketReader *input_color_operation_;

  float offset_[3];
  float power_[3];
  float slope_[3];

 public:
  /**
   * Default constructor
   */
  ColorBalanceASCCDLOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

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
