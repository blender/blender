/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class SunBeamsOperation : public MultiThreadedOperation {
 public:
  SunBeamsOperation();

  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void set_data(const NodeSunBeams &data)
  {
    data_ = data;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

 private:
  void calc_rays_common_data();

 private:
  NodeSunBeams data_;

  float source_px_[2];
  float ray_length_px_;
};

}  // namespace blender::compositor
