/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CalculateStandardDeviationOperation.h"

#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

float CalculateStandardDeviationOperation::calculate_value(const MemoryBuffer *input) const
{
  const float mean = this->calculate_mean(input);

  PixelsSum total = {0};
  exec_system_->execute_work<PixelsSum>(
      input->get_rect(),
      [=](const rcti &split) { return this->calc_area_sum(input, split, mean); },
      total,
      [](PixelsSum &join, const PixelsSum &chunk) {
        join.sum += chunk.sum;
        join.num_pixels += chunk.num_pixels;
      });

  return total.num_pixels <= 1 ? 0.0f : sqrt(total.sum / float(total.num_pixels - 1));
}

using PixelsSum = CalculateMeanOperation::PixelsSum;
PixelsSum CalculateStandardDeviationOperation::calc_area_sum(const MemoryBuffer *input,
                                                             const rcti &area,
                                                             const float mean) const
{
  PixelsSum result = {0};
  for (const float *elem : input->get_buffer_area(area)) {
    if (elem[3] <= 0.0f) {
      continue;
    }
    const float value = setting_func_(elem);
    result.sum += (value - mean) * (value - mean);
    result.num_pixels++;
  }
  return result;
}

}  // namespace blender::compositor
