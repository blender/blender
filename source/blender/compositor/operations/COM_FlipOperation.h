/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class FlipOperation : public MultiThreadedOperation {
 private:
  bool flip_x_;
  bool flip_y_;

 public:
  FlipOperation();

  void setFlipX(bool flipX)
  {
    flip_x_ = flipX;
  }
  void setFlipY(bool flipY)
  {
    flip_y_ = flipY;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
