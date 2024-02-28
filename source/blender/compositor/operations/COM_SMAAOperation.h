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
  float threshold_;
  float contrast_limit_;

 public:
  SMAAEdgeDetectionOperation();

  void set_threshold(float threshold);

  void set_local_contrast_adaptation_factor(float factor);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

/*-----------------------------------------------------------------------------*/
/*  Blending Weight Calculation (Second Pass) */

class SMAABlendingWeightCalculationOperation : public MultiThreadedOperation {
 private:
  std::function<void(int x, int y, float *out)> sample_image_fn_;
  int corner_rounding_;

 public:
  SMAABlendingWeightCalculationOperation();

  void set_corner_rounding(float rounding);

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
 public:
  SMAANeighborhoodBlendingOperation();

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
