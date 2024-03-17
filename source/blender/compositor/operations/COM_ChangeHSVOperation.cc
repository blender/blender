/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChangeHSVOperation.h"

namespace blender::compositor {

ChangeHSVOperation::ChangeHSVOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void ChangeHSVOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float hue = *it.in(1);
    it.out[0] = color[0] + (hue - 0.5f);
    if (it.out[0] > 1.0f) {
      it.out[0] -= 1.0f;
    }
    else if (it.out[0] < 0.0f) {
      it.out[0] += 1.0f;
    }
    const float saturation = *it.in(2);
    const float value = *it.in(3);
    it.out[1] = color[1] * saturation;
    it.out[2] = color[2] * value;
    it.out[3] = color[3];
  }
}

}  // namespace blender::compositor
