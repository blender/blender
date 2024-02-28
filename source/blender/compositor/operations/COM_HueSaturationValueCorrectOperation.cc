/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_HueSaturationValueCorrectOperation.h"

#include "BLI_math_vector.h"

#include "BKE_colortools.hh"

namespace blender::compositor {

HueSaturationValueCorrectOperation::HueSaturationValueCorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void HueSaturationValueCorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                      const rcti &area,
                                                                      Span<MemoryBuffer *> inputs)
{
  float hsv[4];
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    copy_v4_v4(hsv, it.in(0));

    /* We parameterize the curve using the hue value. */
    const float parameter = hsv[0];

    /* Adjust hue, scaling returned default 0.5 up to 1. */
    float f = BKE_curvemapping_evaluateF(curve_mapping_, 0, parameter);
    hsv[0] += f - 0.5f;

    /* Adjust saturation, scaling returned default 0.5 up to 1. */
    f = BKE_curvemapping_evaluateF(curve_mapping_, 1, parameter);
    hsv[1] *= (f * 2.0f);

    /* Adjust value, scaling returned default 0.5 up to 1. */
    f = BKE_curvemapping_evaluateF(curve_mapping_, 2, parameter);
    hsv[2] *= (f * 2.0f);

    hsv[0] = hsv[0] - floorf(hsv[0]); /* Mod 1.0. */
    CLAMP(hsv[1], 0.0f, 1.0f);

    copy_v4_v4(it.out, hsv);
  }
}

}  // namespace blender::compositor
