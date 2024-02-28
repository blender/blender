/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

// #define COM_DEFOCUS_SEARCH

class VariableSizeBokehBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int BOKEH_INPUT_INDEX = 1;
  static constexpr int SIZE_INPUT_INDEX = 2;
  static constexpr int BOUNDING_BOX_INPUT_INDEX = 3;
#ifdef COM_DEFOCUS_SEARCH
  static constexpr int DEFOCUS_INPUT_INDEX = 4;
#endif

  int max_blur_;
  float threshold_;
  bool do_size_scale_; /* scale size, matching 'BokehBlurNode' */

 public:
  VariableSizeBokehBlurOperation();

  void init_execution() override;

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

 public:
  static const int DIVIDER = 4;

  InverseSearchRadiusOperation();

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_max_blur(int max_radius)
  {
    max_blur_ = max_radius;
  }
};
#endif

}  // namespace blender::compositor
