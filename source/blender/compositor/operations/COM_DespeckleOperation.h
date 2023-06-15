/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class DespeckleOperation : public MultiThreadedOperation {
 private:
  constexpr static int IMAGE_INPUT_INDEX = 0;
  constexpr static int FACTOR_INPUT_INDEX = 1;

  float threshold_;
  float threshold_neighbor_;

  // int filter_width_;
  // int filter_height_;

 protected:
  SocketReader *input_operation_;
  SocketReader *input_value_operation_;

 public:
  DespeckleOperation();
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void set_threshold(float threshold)
  {
    threshold_ = threshold;
  }
  void set_threshold_neighbor(float threshold)
  {
    threshold_neighbor_ = threshold;
  }

  void init_execution() override;
  void deinit_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
