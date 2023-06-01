/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

//#define COM_DEFOCUS_SEARCH

class VariableSizeBokehBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int BOKEH_INPUT_INDEX = 1;
  static constexpr int SIZE_INPUT_INDEX = 2;
#ifdef COM_DEFOCUS_SEARCH
  static constexpr int DEFOCUS_INPUT_INDEX = 3;
#endif

  int max_blur_;
  float threshold_;
  bool do_size_scale_; /* scale size, matching 'BokehBlurNode' */
  SocketReader *input_program_;
  SocketReader *input_bokeh_program_;
  SocketReader *input_size_program_;
#ifdef COM_DEFOCUS_SEARCH
  SocketReader *input_search_program_;
#endif

 public:
  VariableSizeBokehBlurOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  void deinitialize_tile_data(rcti *rect, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void set_max_blur(int max_radius)
  {
    max_blur_ = max_radius;
  }

  void set_threshold(float threshold)
  {
    threshold_ = threshold;
  }

  void set_do_scale_size(bool scale_size)
  {
    do_size_scale_ = scale_size;
  }

  void execute_opencl(OpenCLDevice *device,
                      MemoryBuffer *output_memory_buffer,
                      cl_mem cl_output_buffer,
                      MemoryBuffer **input_memory_buffers,
                      std::list<cl_mem> *cl_mem_to_clean_up,
                      std::list<cl_kernel> *cl_kernels_to_clean_up) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

/* Currently unused. If ever used, it needs full-frame implementation. */
#ifdef COM_DEFOCUS_SEARCH
class InverseSearchRadiusOperation : public NodeOperation {
 private:
  int max_blur_;
  SocketReader *input_radius_;

 public:
  static const int DIVIDER = 4;

  InverseSearchRadiusOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_chunk(float output[4], int x, int y, void *data);

  /**
   * Initialize the execution
   */
  void init_execution() override;
  void *initialize_tile_data(rcti *rect) override;
  void deinitialize_tile_data(rcti *rect, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_max_blur(int max_radius)
  {
    max_blur_ = max_radius;
  }
};
#endif

}  // namespace blender::compositor
