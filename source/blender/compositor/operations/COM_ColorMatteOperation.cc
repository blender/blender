/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorMatteOperation.h"

namespace blender::compositor {

ColorMatteOperation::ColorMatteOperation()
{
  add_input_socket(DataType::Color);
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  input_image_program_ = nullptr;
  input_key_program_ = nullptr;
  flags_.can_be_constant = true;
}

void ColorMatteOperation::init_execution()
{
  input_image_program_ = this->get_input_socket_reader(0);
  input_key_program_ = this->get_input_socket_reader(1);
}

void ColorMatteOperation::deinit_execution()
{
  input_image_program_ = nullptr;
  input_key_program_ = nullptr;
}

void ColorMatteOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float in_color[4];
  float in_key[4];

  const float hue = settings_->t1;
  const float sat = settings_->t2;
  const float val = settings_->t3;

  float h_wrap;

  input_image_program_->read_sampled(in_color, x, y, sampler);
  input_key_program_->read_sampled(in_key, x, y, sampler);

  /* Store matte(alpha) value in [0] to go with
   * COM_SetAlphaMultiplyOperation and the Value output.
   */

  if (
      /* Do hue last because it needs to wrap, and does some more checks. */

      /* sat */ (fabsf(in_color[1] - in_key[1]) < sat) &&
      /* val */ (fabsf(in_color[2] - in_key[2]) < val) &&

      /* multiply by 2 because it wraps on both sides of the hue,
       * otherwise 0.5 would key all hue's */

      /* hue */
      ((h_wrap = 2.0f * fabsf(in_color[0] - in_key[0])) < hue || (2.0f - h_wrap) < hue))
  {
    output[0] = 0.0f; /* make transparent */
  }

  else {                     /* Pixel is outside key color. */
    output[0] = in_color[3]; /* Make pixel just as transparent as it was before. */
  }
}

void ColorMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  const float hue = settings_->t1;
  const float sat = settings_->t2;
  const float val = settings_->t3;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_color = it.in(0);
    const float *in_key = it.in(1);

    /* Store matte(alpha) value in [0] to go with
     * COM_SetAlphaMultiplyOperation and the Value output.
     */

    float h_wrap;
    if (
        /* Do hue last because it needs to wrap, and does some more checks. */

        /* #sat */ (fabsf(in_color[1] - in_key[1]) < sat) &&
        /* #val */ (fabsf(in_color[2] - in_key[2]) < val) &&

        /* Multiply by 2 because it wraps on both sides of the hue,
         * otherwise 0.5 would key all hue's. */

        /* #hue */
        ((h_wrap = 2.0f * fabsf(in_color[0] - in_key[0])) < hue || (2.0f - h_wrap) < hue))
    {
      it.out[0] = 0.0f; /* Make transparent. */
    }

    else {                     /* Pixel is outside key color. */
      it.out[0] = in_color[3]; /* Make pixel just as transparent as it was before. */
    }
  }
}

}  // namespace blender::compositor
