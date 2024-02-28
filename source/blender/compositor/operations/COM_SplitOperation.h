/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class SplitOperation : public MultiThreadedOperation {
 private:
  float split_percentage_;
  bool x_split_;

 public:
  SplitOperation();
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void set_split_percentage(float split_percentage)
  {
    split_percentage_ = split_percentage;
  }
  void set_xsplit(bool xsplit)
  {
    x_split_ = xsplit;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
