/* SPDX-FileCopyrightText: 2020 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

class ExposureOperation : public MultiThreadedRowOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;
  SocketReader *input_exposure_program_;

 public:
  ExposureOperation();

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

  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
