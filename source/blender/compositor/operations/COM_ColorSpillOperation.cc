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

#include "COM_ColorSpillOperation.h"
#define AVG(a, b) ((a + b) / 2)

namespace blender::compositor {

ColorSpillOperation::ColorSpillOperation()
{
  addInputSocket(DataType::Color);
  addInputSocket(DataType::Value);
  addOutputSocket(DataType::Color);

  this->m_inputImageReader = nullptr;
  this->m_inputFacReader = nullptr;
  this->m_spillChannel = 1; /* GREEN */
  this->m_spillMethod = 0;
  flags.can_be_constant = true;
}

void ColorSpillOperation::initExecution()
{
  this->m_inputImageReader = this->getInputSocketReader(0);
  this->m_inputFacReader = this->getInputSocketReader(1);
  if (this->m_spillChannel == 0) {
    this->m_rmut = -1.0f;
    this->m_gmut = 1.0f;
    this->m_bmut = 1.0f;
    this->m_channel2 = 1;
    this->m_channel3 = 2;
    if (this->m_settings->unspill == 0) {
      this->m_settings->uspillr = 1.0f;
      this->m_settings->uspillg = 0.0f;
      this->m_settings->uspillb = 0.0f;
    }
  }
  else if (this->m_spillChannel == 1) {
    this->m_rmut = 1.0f;
    this->m_gmut = -1.0f;
    this->m_bmut = 1.0f;
    this->m_channel2 = 0;
    this->m_channel3 = 2;
    if (this->m_settings->unspill == 0) {
      this->m_settings->uspillr = 0.0f;
      this->m_settings->uspillg = 1.0f;
      this->m_settings->uspillb = 0.0f;
    }
  }
  else {
    this->m_rmut = 1.0f;
    this->m_gmut = 1.0f;
    this->m_bmut = -1.0f;

    this->m_channel2 = 0;
    this->m_channel3 = 1;
    if (this->m_settings->unspill == 0) {
      this->m_settings->uspillr = 0.0f;
      this->m_settings->uspillg = 0.0f;
      this->m_settings->uspillb = 1.0f;
    }
  }
}

void ColorSpillOperation::deinitExecution()
{
  this->m_inputImageReader = nullptr;
  this->m_inputFacReader = nullptr;
}

void ColorSpillOperation::executePixelSampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float fac[4];
  float input[4];
  this->m_inputFacReader->readSampled(fac, x, y, sampler);
  this->m_inputImageReader->readSampled(input, x, y, sampler);
  float rfac = MIN2(1.0f, fac[0]);
  float map;

  switch (this->m_spillMethod) {
    case 0: /* simple */
      map = rfac * (input[this->m_spillChannel] -
                    (this->m_settings->limscale * input[this->m_settings->limchan]));
      break;
    default: /* average */
      map = rfac *
            (input[this->m_spillChannel] -
             (this->m_settings->limscale * AVG(input[this->m_channel2], input[this->m_channel3])));
      break;
  }

  if (map > 0.0f) {
    output[0] = input[0] + this->m_rmut * (this->m_settings->uspillr * map);
    output[1] = input[1] + this->m_gmut * (this->m_settings->uspillg * map);
    output[2] = input[2] + this->m_bmut * (this->m_settings->uspillb * map);
    output[3] = input[3];
  }
  else {
    copy_v4_v4(output, input);
  }
}

void ColorSpillOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float factor = MIN2(1.0f, *it.in(1));

    float map;
    switch (m_spillMethod) {
      case 0: /* simple */
        map = factor *
              (color[m_spillChannel] - (m_settings->limscale * color[m_settings->limchan]));
        break;
      default: /* average */
        map = factor * (color[m_spillChannel] -
                        (m_settings->limscale * AVG(color[m_channel2], color[m_channel3])));
        break;
    }

    if (map > 0.0f) {
      it.out[0] = color[0] + m_rmut * (m_settings->uspillr * map);
      it.out[1] = color[1] + m_gmut * (m_settings->uspillg * map);
      it.out[2] = color[2] + m_bmut * (m_settings->uspillb * map);
      it.out[3] = color[3];
    }
    else {
      copy_v4_v4(it.out, color);
    }
  }
}

}  // namespace blender::compositor
