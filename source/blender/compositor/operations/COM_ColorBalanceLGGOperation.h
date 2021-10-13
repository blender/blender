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
class ColorBalanceLGGOperation : public MultiThreadedRowOperation {
 protected:
  /**
   * Prefetched reference to the input_program
   */
  SocketReader *input_value_operation_;
  SocketReader *input_color_operation_;

  float gain_[3];
  float lift_[3];
  float gamma_inv_[3];

 public:
  /**
   * Default constructor
   */
  ColorBalanceLGGOperation();

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
