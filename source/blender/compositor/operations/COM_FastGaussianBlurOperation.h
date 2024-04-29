/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_BlurBaseOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

class FastGaussianBlurOperation : public BlurBaseOperation {
 private:
  float sx_;
  float sy_;
  MemoryBuffer *iirgaus_;

 public:
  FastGaussianBlurOperation();

  static void IIR_gauss(MemoryBuffer *src, float sigma, unsigned int channel, unsigned int xy);
  void init_data() override;
  void deinit_execution() override;
  void init_execution() override;

  void set_size(int size_x, int size_y);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                    const rcti & /*area*/,
                                    Span<MemoryBuffer *> /*inputs*/) override
  {
  }
};

}  // namespace blender::compositor
