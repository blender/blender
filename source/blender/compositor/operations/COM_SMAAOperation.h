/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

class SMAAOperation : public NodeOperation {
 protected:
  float threshold_ = 0.1f;
  float local_contrast_adaptation_factor_ = 2.0f;
  int corner_rounding_ = 25;

 public:
  SMAAOperation();

  void set_threshold(float threshold)
  {
    threshold_ = threshold;
  }

  void set_local_contrast_adaptation_factor(float factor)
  {
    local_contrast_adaptation_factor_ = factor;
  }

  void set_corner_rounding(int corner_rounding)
  {
    corner_rounding_ = corner_rounding;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
