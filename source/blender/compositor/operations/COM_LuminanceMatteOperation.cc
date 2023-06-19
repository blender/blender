/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_LuminanceMatteOperation.h"

#include "IMB_colormanagement.h"

namespace blender::compositor {

LuminanceMatteOperation::LuminanceMatteOperation()
{
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  input_image_program_ = nullptr;
  flags_.can_be_constant = true;
}

void LuminanceMatteOperation::init_execution()
{
  input_image_program_ = this->get_input_socket_reader(0);
}

void LuminanceMatteOperation::deinit_execution()
{
  input_image_program_ = nullptr;
}

void LuminanceMatteOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float in_color[4];
  input_image_program_->read_sampled(in_color, x, y, sampler);

  const float high = settings_->t1;
  const float low = settings_->t2;
  const float luminance = IMB_colormanagement_get_luminance(in_color);

  float alpha;

  /* one line thread-friend algorithm:
   * output[0] = MIN2(input_value[3], MIN2(1.0f, MAX2(0.0f, ((luminance - low) / (high - low))));
   */

  /* test range */
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

  /* don't make something that was more transparent less transparent */
  output[0] = min_ff(alpha, in_color[3]);
}

void LuminanceMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float luminance = IMB_colormanagement_get_luminance(color);

    /* One line thread-friend algorithm:
     * `it.out[0] = MIN2(color[3], MIN2(1.0f, MAX2(0.0f, ((luminance - low) / (high - low))));`
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
    it.out[0] = MIN2(alpha, color[3]);
  }
}

}  // namespace blender::compositor
