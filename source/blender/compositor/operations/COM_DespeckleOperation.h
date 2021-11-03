/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

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
