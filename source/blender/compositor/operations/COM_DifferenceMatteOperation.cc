/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DifferenceMatteOperation.h"

namespace blender::compositor {

DifferenceMatteOperation::DifferenceMatteOperation()
{
  add_input_socket(DataType::Color);
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  flags_.can_be_constant = true;
}

void DifferenceMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color1 = it.in(0);
    const float *color2 = it.in(1);

    float difference = (fabsf(color2[0] - color1[0]) + fabsf(color2[1] - color1[1]) +
                        fabsf(color2[2] - color1[2]));

    /* Average together the distances. */
    difference = difference / 3.0f;

    const float tolerance = settings_->t1;
    const float falloff = settings_->t2;

    /* Make 100% transparent. */
    if (difference <= tolerance) {
      it.out[0] = 0.0f;
    }
    /* In the falloff region, make partially transparent. */
    else if (difference <= falloff + tolerance) {
      difference = difference - tolerance;
      const float alpha = difference / falloff;
      /* Only change if more transparent than before. */
      if (alpha < color1[3]) {
        it.out[0] = alpha;
      }
      else { /* Leave as before. */
        it.out[0] = color1[3];
      }
    }
    else {
      /* Foreground object. */
      it.out[0] = color1[3];
    }
  }
}

}  // namespace blender::compositor
