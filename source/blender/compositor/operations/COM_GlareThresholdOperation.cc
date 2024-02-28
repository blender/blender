/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareThresholdOperation.h"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

GlareThresholdOperation::GlareThresholdOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::FitAny);
  this->add_output_socket(DataType::Color);

  flags_.can_be_constant = true;
}

void GlareThresholdOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  const int width = BLI_rcti_size_x(&r_area) / (1 << settings_->quality);
  const int height = BLI_rcti_size_y(&r_area) / (1 << settings_->quality);
  r_area.xmax = r_area.xmin + width;
  r_area.ymax = r_area.ymin + height;
}

void GlareThresholdOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const float threshold = settings_->threshold;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    if (IMB_colormanagement_get_luminance(color) >= threshold) {
      it.out[0] = color[0] - threshold;
      it.out[1] = color[1] - threshold;
      it.out[2] = color[2] - threshold;

      CLAMP3_MIN(it.out, 0.0f);
    }
    else {
      zero_v3(it.out);
    }
  }
}

}  // namespace blender::compositor
