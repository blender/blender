/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChromaMatteOperation.h"

namespace blender::compositor {

ChromaMatteOperation::ChromaMatteOperation()
{
  add_input_socket(DataType::Color);
  add_input_socket(DataType::Color);
  add_output_socket(DataType::Value);

  flags_.can_be_constant = true;
}

void ChromaMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  const float acceptance = settings_->t1; /* In radians. */
  const float cutoff = settings_->t2;     /* In radians. */
  const float gain = settings_->fstrength;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_image = it.in(0);
    const float *in_key = it.in(1);

    /* Store matte(alpha) value in [0] to go with
     * #COM_SetAlphaMultiplyOperation and the Value output. */

    /* Algorithm from book "Video Demystified", does not include the spill reduction part. */
    /* Find theta, the angle that the color space should be rotated based on key. */

    /* Rescale to `-1.0..1.0`. */
    // const float image_Y = (in_image[0] * 2.0f) - 1.0f;  // UNUSED
    const float image_cb = (in_image[1] * 2.0f) - 1.0f;
    const float image_cr = (in_image[2] * 2.0f) - 1.0f;

    // const float key_Y = (in_key[0] * 2.0f) - 1.0f;  // UNUSED
    const float key_cb = (in_key[1] * 2.0f) - 1.0f;
    const float key_cr = (in_key[2] * 2.0f) - 1.0f;

    const float theta = atan2(key_cr, key_cb);

    /* Rotate the cb and cr into x/z space. */
    const float x_angle = image_cb * cosf(theta) + image_cr * sinf(theta);
    const float z_angle = image_cr * cosf(theta) - image_cb * sinf(theta);

    /* If within the acceptance angle. */
    /* If kfg is <0 then the pixel is outside of the key color. */
    const float kfg = x_angle - (fabsf(z_angle) / tanf(acceptance / 2.0f));

    if (kfg > 0.0f) { /* Found a pixel that is within key color. */
      const float beta = atan2(z_angle, x_angle);
      float alpha = 1.0f - (kfg / gain);

      /* Ff beta is within the cutoff angle. */
      if (fabsf(beta) < (cutoff / 2.0f)) {
        alpha = 0.0f;
      }

      /* Don't make something that was more transparent less transparent. */
      it.out[0] = alpha < in_image[3] ? alpha : in_image[3];
    }
    else {                     /* Pixel is outside key color. */
      it.out[0] = in_image[3]; /* Make pixel just as transparent as it was before. */
    }
  }
}

}  // namespace blender::compositor
