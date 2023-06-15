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
class ChangeHSVOperation : public MultiThreadedOperation {
 private:
  SocketReader *input_operation_;
  SocketReader *hue_operation_;
  SocketReader *saturation_operation_;
  SocketReader *value_operation_;

 public:
  /**
   * Default constructor
   */
  ChangeHSVOperation();

  void init_execution() override;
  void deinit_execution() override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
