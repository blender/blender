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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_ChannelMatteOperation.h"

namespace blender::compositor {

ChannelMatteOperation::ChannelMatteOperation()
{
  addInputSocket(DataType::Color);
  addOutputSocket(DataType::Value);

  m_inputImageProgram = nullptr;
  flags.can_be_constant = true;
}

void ChannelMatteOperation::initExecution()
{
  m_inputImageProgram = this->getInputSocketReader(0);

  m_limit_range = m_limit_max - m_limit_min;

  switch (m_limit_method) {
    /* SINGLE */
    case 0: {
      /* 123 / RGB / HSV / YUV / YCC */
      const int matte_channel = m_matte_channel - 1;
      const int limit_channel = m_limit_channel - 1;
      m_ids[0] = matte_channel;
      m_ids[1] = limit_channel;
      m_ids[2] = limit_channel;
      break;
    }
    /* MAX */
    case 1: {
      switch (m_matte_channel) {
        case 1: {
          m_ids[0] = 0;
          m_ids[1] = 1;
          m_ids[2] = 2;
          break;
        }
        case 2: {
          m_ids[0] = 1;
          m_ids[1] = 0;
          m_ids[2] = 2;
          break;
        }
        case 3: {
          m_ids[0] = 2;
          m_ids[1] = 0;
          m_ids[2] = 1;
          break;
        }
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

void ChannelMatteOperation::deinitExecution()
{
  m_inputImageProgram = nullptr;
}

void ChannelMatteOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float inColor[4];
  float alpha;

  const float limit_max = m_limit_max;
  const float limit_min = m_limit_min;
  const float limit_range = m_limit_range;

  m_inputImageProgram->readSampled(inColor, x, y, sampler);

  /* matte operation */
  alpha = inColor[m_ids[0]] - MAX2(inColor[m_ids[1]], inColor[m_ids[2]]);

  /* flip because 0.0 is transparent, not 1.0 */
  alpha = 1.0f - alpha;

  /* test range */
  if (alpha > limit_max) {
    alpha = inColor[3]; /* Whatever it was prior. */
  }
  else if (alpha < limit_min) {
    alpha = 0.0f;
  }
  else { /* Blend. */
    alpha = (alpha - limit_min) / limit_range;
  }

  /* Store matte(alpha) value in [0] to go with
   * COM_SetAlphaMultiplyOperation and the Value output.
   */

  /* Don't make something that was more transparent less transparent. */
  output[0] = MIN2(alpha, inColor[3]);
}

void ChannelMatteOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);

    /* Matte operation. */
    float alpha = color[m_ids[0]] - MAX2(color[m_ids[1]], color[m_ids[2]]);

    /* Flip because 0.0 is transparent, not 1.0. */
    alpha = 1.0f - alpha;

    /* Test range. */
    if (alpha > m_limit_max) {
      alpha = color[3]; /* Whatever it was prior. */
    }
    else if (alpha < m_limit_min) {
      alpha = 0.0f;
    }
    else { /* Blend. */
      alpha = (alpha - m_limit_min) / m_limit_range;
    }

    /* Store matte(alpha) value in [0] to go with
     * COM_SetAlphaMultiplyOperation and the Value output.
     */

    /* Don't make something that was more transparent less transparent. */
    *it.out = MIN2(alpha, color[3]);
  }
}

}  // namespace blender::compositor
