/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

class DirectionalBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  const NodeDBlurData *data_;

  float center_x_pix_, center_y_pix_;
  float tx_, ty_;
  float sc_, rot_;

 public:
  DirectionalBlurOperation();

  void init_execution() override;

  void set_data(const NodeDBlurData *data)
  {
    data_ = data;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
