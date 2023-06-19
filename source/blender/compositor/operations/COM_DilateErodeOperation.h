/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  /* DilateErode Distance Threshold */
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
  /* Dilate Distance. */
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
  /* Erode Distance */
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
  /* Dilate step */
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
  /** Erode step. */
  ErodeStepOperation();

  void *initialize_tile_data(rcti *rect) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
