/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "COM_MultiThreadedOperation.h"

#include "BLI_listbase.h"

namespace blender::compositor {

/**
 * Class with implementation of keying node
 */
class KeyingOperation : public MultiThreadedOperation {
 protected:
  float screen_balance_;

 public:
  KeyingOperation();

  void set_screen_balance(float value)
  {
    screen_balance_ = value;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
