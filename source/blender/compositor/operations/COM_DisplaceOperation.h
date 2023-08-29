/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class DisplaceOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_color_program_;

  float width_x4_;
  float height_x4_;

  int input_vector_width_;
  int input_vector_height_;

  std::function<void(float x, float y, float *out)> vector_read_fn_;
  std::function<void(float x, float y, float *out)> scale_x_read_fn_;
  std::function<void(float x, float y, float *out)> scale_y_read_fn_;

 public:
  DisplaceOperation();

  /**
   * we need a 2x2 differential filter for Vector Input and full buffer for the image
   */
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void pixel_transform(const float xy[2], float r_uv[2], float r_deriv[2][2]);

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  bool read_displacement(
      float x, float y, float xscale, float yscale, const float origin[2], float &r_u, float &r_v);
};

}  // namespace blender::compositor
