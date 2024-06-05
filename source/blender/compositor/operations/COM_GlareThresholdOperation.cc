/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

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
    float4 hsva;
    rgb_to_hsv_v(it.in(0), hsva);

    hsva.z = math::max(0.0f, hsva.z - threshold);

    hsv_to_rgb_v(hsva, it.out);
    CLAMP3_MIN(it.out, 0.0f);
    it.out[3] = 1.0f;
  }
}

}  // namespace blender::compositor
