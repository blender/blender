/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class KuwaharaAnisotropicOperation : public MultiThreadedOperation {
  SocketReader *image_reader_;
  SocketReader *s_xx_reader_;
  SocketReader *s_yy_reader_;
  SocketReader *s_xy_reader_;

  int kernel_size_;
  int n_div_;

 public:
  KuwaharaAnisotropicOperation();

  void init_execution() override;
  void deinit_execution() override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void set_kernel_size(int kernel_size);
  int get_kernel_size();
  int get_n_div();

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
