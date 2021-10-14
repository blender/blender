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

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class MapUVOperation : public MultiThreadedOperation {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int UV_INPUT_INDEX = 1;
  /**
   * Cached reference to the input_program
   */
  SocketReader *inputUVProgram_;
  SocketReader *input_color_program_;

  int uv_width_;
  int uv_height_;
  int image_width_;
  int image_height_;

  float alpha_;

  std::function<void(float x, float y, float *out)> uv_input_read_fn_;

 public:
  MapUVOperation();

  /**
   * we need a 3x3 differential filter for UV Input and full buffer for the image
   */
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void pixel_transform(const float xy[2], float r_uv[2], float r_deriv[2][2], float &r_alpha);

  void init_data() override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_alpha(float alpha)
  {
    alpha_ = alpha;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  bool read_uv(float x, float y, float &r_u, float &r_v, float &r_alpha);
};

}  // namespace blender::compositor
