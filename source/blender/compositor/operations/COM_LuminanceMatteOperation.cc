/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_LuminanceMatteOperation.h"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

LuminanceMatteOperation::LuminanceMatteOperation()
{
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  flags_.can_be_constant = true;
}

void LuminanceMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float luminance = IMB_colormanagement_get_luminance(color);

    /* One line thread-friend algorithm:
     * `it.out[0] = std::min(color[3], std::min(1.0f, std::max(0.0f, ((luminance - low) / (high -
     * low))));`
     */

    /* Test range. */
    const float high = settings_->t1;
    const float low = settings_->t2;
    float alpha;
    if (luminance > high) {
      alpha = 1.0f;
    }
    else if (luminance < low) {
      alpha = 0.0f;
    }
    else { /* Blend. */
      alpha = (luminance - low) / (high - low);
    }

    /* Store matte(alpha) value in [0] to go with
     * COM_SetAlphaMultiplyOperation and the Value output.
     */

    /* Don't make something that was more transparent less transparent. */
    it.out[0] = std::min(alpha, color[3]);
  }
}

}  // namespace blender::compositor
