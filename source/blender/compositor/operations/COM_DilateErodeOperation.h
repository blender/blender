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

class DilateErodeThresholdOperation : public MultiThreadedOperation {
 public:
  struct PixelData;

 private:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;

  float distance_;
  float switch_;
  float inset_;

  /**
   * determines the area of interest to track pixels
   * keep this one as small as possible for speed gain.
   */
  int scope_;

 public:
  DilateErodeThresholdOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_data() override;
  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_distance(float distance)
  {
    distance_ = distance;
  }
  void set_switch(float sw)
  {
    switch_ = sw;
  }
  void set_inset(float inset)
  {
    inset_ = inset;
  }

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class DilateDistanceOperation : public MultiThreadedOperation {
 public:
  struct PixelData;

 protected:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;
  float distance_;
  int scope_;

 public:
  DilateDistanceOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_data() override;
  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_distance(float distance)
  {
    distance_ = distance;
  }
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void execute_opencl(OpenCLDevice *device,
                      MemoryBuffer *output_memory_buffer,
                      cl_mem cl_output_buffer,
                      MemoryBuffer **input_memory_buffers,
                      std::list<cl_mem> *cl_mem_to_clean_up,
                      std::list<cl_kernel> *cl_kernels_to_clean_up) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class ErodeDistanceOperation : public DilateDistanceOperation {
 public:
  ErodeDistanceOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  void execute_opencl(OpenCLDevice *device,
                      MemoryBuffer *output_memory_buffer,
                      cl_mem cl_output_buffer,
                      MemoryBuffer **input_memory_buffers,
                      std::list<cl_mem> *cl_mem_to_clean_up,
                      std::list<cl_kernel> *cl_kernels_to_clean_up) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class DilateStepOperation : public MultiThreadedOperation {
 protected:
  /**
   * Cached reference to the input_program
   */
  SocketReader *input_program_;

  int iterations_;

 public:
  DilateStepOperation();

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
  void deinitialize_tile_data(rcti *rect, void *data) override;

  void set_iterations(int iterations)
  {
    iterations_ = iterations;
  }

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class ErodeStepOperation : public DilateStepOperation {
 public:
  ErodeStepOperation();

  void *initialize_tile_data(rcti *rect) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
