/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_texture_types.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class MapRangeOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_operation_;
  SocketReader *source_min_operation_;
  SocketReader *source_max_operation_;
  SocketReader *dest_min_operation_;
  SocketReader *dest_max_operation_;

  bool use_clamp_;

 public:
  /**
   * Default constructor
   */
  MapRangeOperation();

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

  /**
   * Clamp the output
   */
  void set_use_clamp(bool value)
  {
    use_clamp_ = value;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
