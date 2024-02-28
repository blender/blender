/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_geom.h"

#include "COM_KeyingDespillOperation.h"

namespace blender::compositor {

KeyingDespillOperation::KeyingDespillOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  despill_factor_ = 0.5f;
  color_balance_ = 0.5f;

  flags_.can_be_constant = true;
}

void KeyingDespillOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *pixel_color = it.in(0);
    const float *screen_color = it.in(1);

    const int screen_primary_channel = max_axis_v3(screen_color);
    const int other_1 = (screen_primary_channel + 1) % 3;
    const int other_2 = (screen_primary_channel + 2) % 3;

    const int min_channel = std::min(other_1, other_2);
    const int max_channel = std::max(other_1, other_2);

    const float average_value = color_balance_ * pixel_color[min_channel] +
                                (1.0f - color_balance_) * pixel_color[max_channel];
    const float amount = (pixel_color[screen_primary_channel] - average_value);

    copy_v4_v4(it.out, pixel_color);

    const float amount_despill = despill_factor_ * amount;
    if (amount_despill > 0.0f) {
      it.out[screen_primary_channel] = pixel_color[screen_primary_channel] - amount_despill;
    }
  }
}

}  // namespace blender::compositor
