/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ColorSpillOperation : public MultiThreadedOperation {
 protected:
  NodeColorspill *settings_;
  SocketReader *input_image_reader_;
  SocketReader *input_fac_reader_;
  int spill_channel_;
  int spill_method_;
  int channel2_;
  int channel3_;
  float rmut_, gmut_, bmut_;

 public:
  /**
   * Default constructor
   */
  ColorSpillOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_execution() override;
  void deinit_execution() override;

  void set_settings(NodeColorspill *node_color_spill)
  {
    settings_ = node_color_spill;
  }
  void set_spill_channel(int channel)
  {
    spill_channel_ = channel;
  }
  void set_spill_method(int method)
  {
    spill_method_ = method;
  }

  float calculate_map_value(float fac, float *input);

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
