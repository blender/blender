/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * \brief Pixelate operation
 *
 * The Tile compositor is by default sub-pixel accurate.
 * For some setups you don want this.
 * This operation will remove the sub-pixel accuracy
 */
class PixelateOperation : public MultiThreadedOperation {
 private:
  /**
   * \brief cached reference to the input operation
   */
  SocketReader *input_operation_;

  int pixel_size_;

 public:
  PixelateOperation();

  void set_pixel_size(const int pixel_size)
  {
    if (pixel_size < 1) {
      pixel_size_ = 1;
      return;
    }
    pixel_size_ = pixel_size;
  }

  /**
   * \brief initialization of the execution
   */
  void init_execution() override;

  /**
   * \brief de-initialization of the execution
   */
  void deinit_execution() override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  /**
   * \brief execute_pixel
   * \param output: result
   * \param x: x-coordinate
   * \param y: y-coordinate
   * \param sampler: sampler
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
