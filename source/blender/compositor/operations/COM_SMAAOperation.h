/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/*-----------------------------------------------------------------------------*/
/* Edge Detection (First Pass) */

class SMAAEdgeDetectionOperation : public MultiThreadedOperation {
 protected:
  SocketReader *image_reader_;
  SocketReader *value_reader_;

  float threshold_;
  float contrast_limit_;

 public:
  SMAAEdgeDetectionOperation();

  /**
   * the inner loop of this program
   */
  virtual void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void set_threshold(float threshold);

  void set_local_contrast_adaptation_factor(float factor);

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

/*-----------------------------------------------------------------------------*/
/*  Blending Weight Calculation (Second Pass) */

class SMAABlendingWeightCalculationOperation : public MultiThreadedOperation {
 private:
  SocketReader *image_reader_;
  std::function<void(int x, int y, float *out)> sample_image_fn_;
  int corner_rounding_;

 public:
  SMAABlendingWeightCalculationOperation();

  /**
   * the inner loop of this program
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

  void set_corner_rounding(float rounding);

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  /* Diagonal Search Functions */
  /**
   * These functions allows to perform diagonal pattern searches.
   */
  int search_diag1(int x, int y, int dir, bool *r_found);
  int search_diag2(int x, int y, int dir, bool *r_found);
  /**
   * This searches for diagonal patterns and returns the corresponding weights.
   */
  void calculate_diag_weights(int x, int y, const float edges[2], float weights[2]);
  bool is_vertical_search_unneeded(int x, int y);

  /* Horizontal/Vertical Search Functions */
  int search_xleft(int x, int y);
  int search_xright(int x, int y);
  int search_yup(int x, int y);
  int search_ydown(int x, int y);

  /*  Corner Detection Functions */
  void detect_horizontal_corner_pattern(
      float weights[2], int left, int right, int y, int d1, int d2);
  void detect_vertical_corner_pattern(
      float weights[2], int x, int top, int bottom, int d1, int d2);
};

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending (Third Pass) */

class SMAANeighborhoodBlendingOperation : public MultiThreadedOperation {
 private:
  SocketReader *image1Reader_;
  SocketReader *image2Reader_;

 public:
  SMAANeighborhoodBlendingOperation();

  /**
   * the inner loop of this program
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

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
