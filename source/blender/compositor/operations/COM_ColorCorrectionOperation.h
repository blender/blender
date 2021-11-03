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
