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
