/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PosterizeOperation.h"

namespace blender::compositor {

PosterizeOperation::PosterizeOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void PosterizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_value = it.in(0);
    const float *in_steps = it.in(1);
    float steps = in_steps[0];
    CLAMP(steps, 2.0f, 1024.0f);
    const float steps_inv = 1.0f / steps;

    it.out[0] = floor(in_value[0] / steps_inv) * steps_inv;
    it.out[1] = floor(in_value[1] / steps_inv) * steps_inv;
    it.out[2] = floor(in_value[2] / steps_inv) * steps_inv;
    it.out[3] = in_value[3];
  }
}

}  // namespace blender::compositor
