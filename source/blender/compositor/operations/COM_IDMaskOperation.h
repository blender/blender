/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class IDMaskOperation : public MultiThreadedOperation {
 private:
  float object_index_;

 public:
  IDMaskOperation();

  void set_object_index(float object_index)
  {
    object_index_ = object_index;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
