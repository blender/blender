/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class PixelateOperation : public MultiThreadedOperation {
 private:
  int pixel_size_;

 public:
  PixelateOperation();

  void set_pixel_size(const int pixel_size)
  {
    if (pixel_size < 1) {
      pixel_size_ = 1;
      return;
    }
    pixel_size_ = pixel_size;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
