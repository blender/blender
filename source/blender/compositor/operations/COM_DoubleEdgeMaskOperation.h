/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class DoubleEdgeMaskOperation : public NodeOperation {
 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_outer_mask_;
  SocketReader *input_inner_mask_;
  bool adjacent_only_;
  bool keep_inside_;

  /* TODO(manzanilla): To be removed with tiled implementation. */
  float *cached_instance_;

  bool is_output_rendered_;

 public:
  DoubleEdgeMaskOperation();

  void do_double_edge_mask(float *imask, float *omask, float *res);
  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void set_adjecent_only(bool adjacent_only)
  {
    adjacent_only_ = adjacent_only;
  }
  void set_keep_inside(bool keep_inside)
  {
    keep_inside_ = keep_inside;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
