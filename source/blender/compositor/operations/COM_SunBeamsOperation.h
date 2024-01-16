/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class SunBeamsOperation : public MultiThreadedOperation {
 public:
  SunBeamsOperation();

  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_execution() override;

  void set_data(const NodeSunBeams &data)
  {
    data_ = data;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

  void deinit_execution() override;

 private:
  NodeSunBeams data_;
  SocketReader *input_program_;
};

}  // namespace blender::compositor
