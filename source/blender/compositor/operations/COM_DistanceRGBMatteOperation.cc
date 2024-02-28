/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DistanceRGBMatteOperation.h"

namespace blender::compositor {

DistanceRGBMatteOperation::DistanceRGBMatteOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);

  flags_.can_be_constant = true;
}

float DistanceRGBMatteOperation::calculate_distance(const float key[4], const float image[4])
{
  return len_v3v3(key, image);
}

void DistanceRGBMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_image = it.in(0);
    const float *in_key = it.in(1);

    float distance = this->calculate_distance(in_key, in_image);
    const float tolerance = settings_->t1;
    const float falloff = settings_->t2;

    /* Store matte(alpha) value in [0] to go with
     * COM_SetAlphaMultiplyOperation and the Value output.
     */

    /* Make 100% transparent. */
    if (distance < tolerance) {
      it.out[0] = 0.0f;
    }
    /* In the falloff region, make partially transparent. */
    else if (distance < falloff + tolerance) {
      distance = distance - tolerance;
      const float alpha = distance / falloff;
      /* Only change if more transparent than before. */
      if (alpha < in_image[3]) {
        it.out[0] = alpha;
      }
      else { /* Leave as before. */
        it.out[0] = in_image[3];
      }
    }
    else {
      /* Leave as before. */
      it.out[0] = in_image[3];
    }
  }
}

}  // namespace blender::compositor
