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

#include "COM_BlurBaseOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

class FastGaussianBlurOperation : public BlurBaseOperation {
 private:
  float sx_;
  float sy_;
  MemoryBuffer *iirgaus_;

 public:
  FastGaussianBlurOperation();
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel(float output[4], int x, int y, void *data) override;

  static void IIR_gauss(MemoryBuffer *src, float sigma, unsigned int channel, unsigned int xy);
  void *initialize_tile_data(rcti *rect) override;
  void init_data() override;
  void deinit_execution() override;
  void init_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                    const rcti &UNUSED(area),
                                    Span<MemoryBuffer *> UNUSED(inputs)) override
  {
  }
};

enum {
  FAST_GAUSS_OVERLAY_MIN = -1,
  FAST_GAUSS_OVERLAY_NONE = 0,
  FAST_GAUSS_OVERLAY_MAX = 1,
};

class FastGaussianBlurValueOperation : public MultiThreadedOperation {
 private:
  float sigma_;
  MemoryBuffer *iirgaus_;
  SocketReader *inputprogram_;

  /**
   * -1: re-mix with darker
   *  0: do nothing
   *  1 re-mix with lighter */
  int overlay_;

 public:
  FastGaussianBlurValueOperation();
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void *initialize_tile_data(rcti *rect) override;
  void deinit_execution() override;
  void init_execution() override;
  void set_sigma(float sigma)
  {
    sigma_ = sigma;
  }

  /* used for DOF blurring ZBuffer */
  void set_overlay(int overlay)
  {
    overlay_ = overlay;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
