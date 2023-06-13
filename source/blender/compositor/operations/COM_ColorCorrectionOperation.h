/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

class ColorCorrectionOperation : public MultiThreadedRowOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_image_;
  SocketReader *input_mask_;
  NodeColorCorrection *data_;

  bool red_channel_enabled_;
  bool green_channel_enabled_;
  bool blue_channel_enabled_;

 public:
  ColorCorrectionOperation();

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

  void set_data(NodeColorCorrection *data)
  {
    data_ = data;
  }
  void set_red_channel_enabled(bool enabled)
  {
    red_channel_enabled_ = enabled;
  }
  void set_green_channel_enabled(bool enabled)
  {
    green_channel_enabled_ = enabled;
  }
  void set_blue_channel_enabled(bool enabled)
  {
    blue_channel_enabled_ = enabled;
  }

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
