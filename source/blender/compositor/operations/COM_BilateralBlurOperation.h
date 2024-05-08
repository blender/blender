/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_base.hh"

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class BilateralBlurOperation : public MultiThreadedOperation {
 private:
  NodeBilateralBlurData *data_;
  int radius_;

 public:
  BilateralBlurOperation();

  void set_data(NodeBilateralBlurData *data)
  {
    data_ = data;
    radius_ = int(math::ceil(data->sigma_space + data->iter));
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
