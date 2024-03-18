/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * Class with implementation of keying despill node
 */
class KeyingDespillOperation : public MultiThreadedOperation {
 protected:
  SocketReader *pixel_reader_;
  SocketReader *screen_reader_;
  float despill_factor_;
  float color_balance_;

 public:
  KeyingDespillOperation();

  void init_execution() override;
  void deinit_execution() override;

  void set_despill_factor(float value)
  {
    despill_factor_ = value;
  }
  void set_color_balance(float value)
  {
    color_balance_ = value;
  }

  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
