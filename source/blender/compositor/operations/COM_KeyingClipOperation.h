/* SPDX-FileCopyrightText: 2012 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * Class with implementation of black/white clipping for keying node
 */
class KeyingClipOperation : public MultiThreadedOperation {
 protected:
  float clip_black_;
  float clip_white_;

  int kernel_radius_;
  float kernel_tolerance_;

  bool is_edge_matte_;

 public:
  KeyingClipOperation();

  void set_clip_black(float value)
  {
    clip_black_ = value;
  }
  void set_clip_white(float value)
  {
    clip_white_ = value;
  }

  void set_kernel_radius(int value)
  {
    kernel_radius_ = value;
  }
  void set_kernel_tolerance(float value)
  {
    kernel_tolerance_ = value;
  }

  void set_is_edge_matte(bool value)
  {
    is_edge_matte_ = value;
  }

  void *initialize_tile_data(rcti *rect) override;

  void execute_pixel(float output[4], int x, int y, void *data) override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
