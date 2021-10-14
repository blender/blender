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
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

class BokehBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  SocketReader *input_program_;
  SocketReader *input_bokeh_program_;
  SocketReader *input_bounding_box_reader_;
  void update_size();
  float size_;
  bool sizeavailable_;

  float bokeh_mid_x_;
  float bokeh_mid_y_;
  float bokehDimension_;
  bool extend_bounds_;

 public:
  BokehBlurOperation();

  void init_data() override;

  void *initialize_tile_data(rcti *rect) override;
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

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void set_size(float size)
  {
    size_ = size;
    sizeavailable_ = true;
  }

  void execute_opencl(OpenCLDevice *device,
                      MemoryBuffer *output_memory_buffer,
                      cl_mem cl_output_buffer,
                      MemoryBuffer **input_memory_buffers,
                      std::list<cl_mem> *cl_mem_to_clean_up,
                      std::list<cl_kernel> *cl_kernels_to_clean_up) override;

  void set_extend_bounds(bool extend_bounds)
  {
    extend_bounds_ = extend_bounds;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
