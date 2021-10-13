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

#include "COM_SetAlphaReplaceOperation.h"

namespace blender::compositor {

SetAlphaReplaceOperation::SetAlphaReplaceOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  inputColor_ = nullptr;
  inputAlpha_ = nullptr;
  this->flags.can_be_constant = true;
}

void SetAlphaReplaceOperation::initExecution()
{
  inputColor_ = getInputSocketReader(0);
  inputAlpha_ = getInputSocketReader(1);
}

void SetAlphaReplaceOperation::executePixelSampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  float alpha_input[4];

  inputColor_->readSampled(output, x, y, sampler);
  inputAlpha_->readSampled(alpha_input, x, y, sampler);
  output[3] = alpha_input[0];
}

void SetAlphaReplaceOperation::deinitExecution()
{
  inputColor_ = nullptr;
  inputAlpha_ = nullptr;
}

void SetAlphaReplaceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                            const rcti &area,
                                                            Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float alpha = *it.in(1);
    copy_v3_v3(it.out, color);
    it.out[3] = alpha;
  }
}

}  // namespace blender::compositor
