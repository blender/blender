/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_texture_types.h"

namespace blender::compositor {

class ColorRampOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;
  ColorBand *color_band_;

 public:
  ColorRampOperation();

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

  void set_color_band(ColorBand *color_band)
  {
    color_band_ = color_band;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
