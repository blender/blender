/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_GaussianBlurBaseOperation.h"

namespace blender::compositor {

/* TODO(manzanilla): everything to be removed with tiled implementation except the constructor. */
class GaussianXBlurOperation : public GaussianBlurBaseOperation {
 private:
  void update_gauss();

 public:
  GaussianXBlurOperation();

  /**
   * \brief The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void execute_opencl(OpenCLDevice *device,
                      MemoryBuffer *output_memory_buffer,
                      cl_mem cl_output_buffer,
                      MemoryBuffer **input_memory_buffers,
                      std::list<cl_mem> *cl_mem_to_clean_up,
                      std::list<cl_kernel> *cl_kernels_to_clean_up) override;

  /**
   * \brief initialize the execution
   */
  void init_execution() override;

  /**
   * \brief Deinitialize the execution
   */
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void check_opencl()
  {
    flags_.open_cl = (data_.sizex >= 128);
  }
};

}  // namespace blender::compositor
