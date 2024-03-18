/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  void set_size(int size_x, int size_y);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                    const rcti & /*area*/,
                                    Span<MemoryBuffer *> /*inputs*/) override
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
   *  1 re-mix with lighter.
   */
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
