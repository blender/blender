/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_light_types.h"

namespace blender::compositor {

class GlareThresholdOperation : public MultiThreadedOperation {
 private:
  /**
   * \brief Cached reference to the input_program
   */
  SocketReader *input_program_;

  /**
   * \brief settings of the glare node.
   */
  const NodeGlare *settings_;

 public:
  GlareThresholdOperation();

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

  void set_glare_settings(const NodeGlare *settings)
  {
    settings_ = settings;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
