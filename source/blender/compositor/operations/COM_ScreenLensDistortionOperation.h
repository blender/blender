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
#include "DNA_node_types.h"

struct RNG;

namespace blender::compositor {

class ScreenLensDistortionOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;
  struct RNG *rng_;

  bool fit_;
  bool jitter_;

  float dispersion_;
  float distortion_;
  bool dispersion_const_;
  bool distortion_const_;
  bool variables_ready_;
  float k_[3];
  float k4_[3];
  float dk4_[3];
  float maxk_;
  float sc_, cx_, cy_;

 public:
  ScreenLensDistortionOperation();

  void init_data() override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_fit(bool fit)
  {
    fit_ = fit;
  }
  void set_jitter(bool jitter)
  {
    jitter_ = jitter;
  }

  /** Set constant distortion value */
  void set_distortion(float distortion);
  /** Set constant dispersion value */
  void set_dispersion(float dispersion);

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void determineUV(float result[6], float x, float y) const;
  void update_variables(float distortion, float dispersion);

  void get_uv(const float xy[2], float uv[2]) const;
  void distort_uv(const float uv[2], float t, float xy[2]) const;
  bool get_delta(float r_sq, float k4, const float uv[2], float delta[2]) const;
  void accumulate(const MemoryBuffer *buffer,
                  int a,
                  int b,
                  float r_sq,
                  const float uv[2],
                  const float delta[3][2],
                  float sum[4],
                  int count[3]) const;
};

}  // namespace blender::compositor
