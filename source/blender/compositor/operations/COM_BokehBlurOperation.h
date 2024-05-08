/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class BokehBlurOperation : public MultiThreadedOperation {
 private:
  void update_size();
  float size_;
  bool sizeavailable_;

  bool extend_bounds_;

 public:
  BokehBlurOperation();

  void init_data() override;

  void set_size(float size)
  {
    size_ = size;
    sizeavailable_ = true;
  }

  void set_extend_bounds(bool extend_bounds)
  {
    extend_bounds_ = extend_bounds;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
