/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KeyingOperation.h"

#include "BLI_math_geom.h"

namespace blender::compositor {

static float get_pixel_saturation(const float pixel_color[4],
                                  float screen_balance,
                                  int primary_channel)
{
  const int other_1 = (primary_channel + 1) % 3;
  const int other_2 = (primary_channel + 2) % 3;

  const int min_channel = std::min(other_1, other_2);
  const int max_channel = std::max(other_1, other_2);

  const float val = pixel_color[max_channel] +
                    screen_balance * (pixel_color[min_channel] - pixel_color[max_channel]);

  return (pixel_color[primary_channel] - val) * fabsf(1.0f - val);
}

KeyingOperation::KeyingOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Value);

  screen_balance_ = 0.5f;

  flags_.can_be_constant = true;
}

void KeyingOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *pixel_color = it.in(0);
    const float *screen_color = it.in(1);

    const int primary_channel = max_axis_v3(screen_color);
    const float min_pixel_color = min_fff(pixel_color[0], pixel_color[1], pixel_color[2]);

    if (min_pixel_color > 1.0f) {
      /* Overexposure doesn't happen on screen itself and usually happens
       * on light sources in the shot, this need to be checked separately
       * because saturation and falloff calculation is based on the fact
       * that pixels are not overexposed.
       */
      it.out[0] = 1.0f;
    }
    else {
      const float saturation = get_pixel_saturation(pixel_color, screen_balance_, primary_channel);
      const float screen_saturation = get_pixel_saturation(
          screen_color, screen_balance_, primary_channel);

      if (saturation < 0) {
        /* Means main channel of pixel is different from screen,
         * assume this is completely a foreground.
         */
        it.out[0] = 1.0f;
      }
      else if (saturation >= screen_saturation) {
        /* Matched main channels and higher saturation on pixel
         * is treated as completely background.
         */
        it.out[0] = 0.0f;
      }
      else {
        /* Nice alpha falloff on edges. */
        const float distance = 1.0f - saturation / screen_saturation;
        it.out[0] = distance;
      }
    }
  }
}

}  // namespace blender::compositor
