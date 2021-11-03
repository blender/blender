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
#include "DNA_node_types.h"
#include <functional>

namespace blender::compositor {

/**
 * \brief base class of CalculateMean, implementing the simple CalculateMean
 * \ingroup operation
 */
class CalculateMeanOperation : public MultiThreadedOperation {
 public:
  struct PixelsSum {
    float sum;
    int num_pixels;
  };

 protected:
  /**
   * \brief Cached reference to the reader
   */
  SocketReader *image_reader_;

  bool iscalculated_;
  float result_;
  int setting_;
  std::function<float(const float *elem)> setting_func_;

 public:
  CalculateMeanOperation();

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

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void set_setting(int setting);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  virtual void update_memory_buffer_started(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;

  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;

 protected:
  void calculate_mean(MemoryBuffer *tile);
  float calc_mean(const MemoryBuffer *input);

 private:
  PixelsSum calc_area_sum(const MemoryBuffer *input, const rcti &area);
};

}  // namespace blender::compositor
