/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConstantOperation.h"

namespace blender::compositor {

class BufferOperation : public ConstantOperation {
 private:
  MemoryBuffer *buffer_;
  MemoryBuffer *inflated_buffer_;

 public:
  BufferOperation(MemoryBuffer *buffer, DataType data_type);

  const float *get_constant_elem() override;
  void *initialize_tile_data(rcti *rect) override;
  void init_execution() override;
  void deinit_execution() override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  void execute_pixel_filtered(
      float output[4], float x, float y, float dx[2], float dy[2]) override;
};

}  // namespace blender::compositor
