/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_ConstantOperation.h"
#include "DNA_node_types.h"
#include <functional>

namespace blender::compositor {

/**
 * \brief base class of CalculateMean, implementing the simple CalculateMean
 * \ingroup operation
 */
class CalculateMeanOperation : public ConstantOperation {
 public:
  struct PixelsSum {
    float sum;
    int num_pixels;
  };

 protected:
  bool is_calculated_;
  float constant_value_;
  int setting_;
  std::function<float(const float *elem)> setting_func_;

 public:
  CalculateMeanOperation();

  void init_execution() override;

  void set_setting(int setting);

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  const float *get_constant_elem() override;
  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 protected:
  /* Calculate value which will be written to the single-element output in the
   * update_memory_buffer().
   * The caller takes care of checking the value is only calculated once. */
  virtual float calculate_value(const MemoryBuffer *input) const;

  float calculate_mean(const MemoryBuffer *input) const;

 private:
  PixelsSum calc_area_sum(const MemoryBuffer *input, const rcti &area) const;
};

}  // namespace blender::compositor
