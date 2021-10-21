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
