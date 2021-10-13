/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_ColorMatteOperation.h"

namespace blender::compositor {

ColorMatteOperation::ColorMatteOperation()
{
  addInputSocket(DataType::Color);
  addInputSocket(DataType::Color);
  addOutputSocket(DataType::Value);

  inputImageProgram_ = nullptr;
  inputKeyProgram_ = nullptr;
  flags.can_be_constant = true;
}

void ColorMatteOperation::initExecution()
{
  inputImageProgram_ = this->getInputSocketReader(0);
  inputKeyProgram_ = this->getInputSocketReader(1);
}

void ColorMatteOperation::deinitExecution()
{
  inputImageProgram_ = nullptr;
  inputKeyProgram_ = nullptr;
}

void ColorMatteOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float inColor[4];
  float inKey[4];

  const float hue = settings_->t1;
  const float sat = settings_->t2;
  const float val = settings_->t3;

  float h_wrap;

  inputImageProgram_->readSampled(inColor, x, y, sampler);
  inputKeyProgram_->readSampled(inKey, x, y, sampler);

  /* Store matte(alpha) value in [0] to go with
   * COM_SetAlphaMultiplyOperation and the Value output.
   */

  if (
      /* Do hue last because it needs to wrap, and does some more checks. */

      /* sat */ (fabsf(inColor[1] - inKey[1]) < sat) &&
      /* val */ (fabsf(inColor[2] - inKey[2]) < val) &&

      /* multiply by 2 because it wraps on both sides of the hue,
       * otherwise 0.5 would key all hue's */

      /* hue */ ((h_wrap = 2.0f * fabsf(inColor[0] - inKey[0])) < hue || (2.0f - h_wrap) < hue)) {
    output[0] = 0.0f; /* make transparent */
  }

  else {                    /* Pixel is outside key color. */
    output[0] = inColor[3]; /* Make pixel just as transparent as it was before. */
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
        ((h_wrap = 2.0f * fabsf(in_color[0] - in_key[0])) < hue || (2.0f - h_wrap) < hue)) {
      it.out[0] = 0.0f; /* Make transparent. */
    }

    else {                     /* Pixel is outside key color. */
      it.out[0] = in_color[3]; /* Make pixel just as transparent as it was before. */
    }
  }
}

}  // namespace blender::compositor
