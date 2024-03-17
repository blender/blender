/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class InvertOperation : public MultiThreadedOperation {
 private:
  bool alpha_;
  bool color_;

 public:
  InvertOperation();

  void set_color(bool color)
  {
    color_ = color;
  }
  void set_alpha(bool alpha)
  {
    alpha_ = alpha;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
