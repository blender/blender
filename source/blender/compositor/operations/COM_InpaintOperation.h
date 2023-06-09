/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

class InpaintSimpleOperation : public NodeOperation {
 protected:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_image_program_;

  int iterations_;

  float *cached_buffer_;
  bool cached_buffer_ready_;

  int *pixelorder_;
  int area_size_;
  short *manhattan_distance_;

 public:
  /** In-paint (simple convolve using average of known pixels). */
  InpaintSimpleOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_iterations(int iterations)
  {
    iterations_ = iterations;
  }

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 private:
  void calc_manhattan_distance();
  void clamp_xy(int &x, int &y);
  float *get_pixel(int x, int y);
  int mdist(int x, int y);
  bool next_pixel(int &x, int &y, int &curr, int iters);
  void pix_step(int x, int y);
};

}  // namespace blender::compositor
