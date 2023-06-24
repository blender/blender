/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class CryptomatteOperation : public MultiThreadedOperation {
 private:
  Vector<float> object_index_;

 public:
  Vector<SocketReader *> inputs;

  CryptomatteOperation(size_t num_inputs = 6);

  void init_execution() override;
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void add_object_index(float object_index);

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
