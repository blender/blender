/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class KuwaharaAnisotropicOperation : public MultiThreadedOperation {
  SocketReader *image_reader_;
  SocketReader *size_reader_;
  SocketReader *structure_tensor_reader_;

 public:
  NodeKuwaharaData data;

  KuwaharaAnisotropicOperation();

  void init_execution() override;
  void deinit_execution() override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  float get_sharpness();
  float get_eccentricity();
};

}  // namespace blender::compositor
