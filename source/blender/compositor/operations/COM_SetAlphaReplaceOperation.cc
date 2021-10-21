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
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  input_color_ = nullptr;
  input_alpha_ = nullptr;
  flags_.can_be_constant = true;
}

void SetAlphaReplaceOperation::init_execution()
{
  input_color_ = get_input_socket_reader(0);
  input_alpha_ = get_input_socket_reader(1);
}

void SetAlphaReplaceOperation::execute_pixel_sampled(float output[4],
                                                     float x,
                                                     float y,
                                                     PixelSampler sampler)
{
  float alpha_input[4];

  input_color_->read_sampled(output, x, y, sampler);
  input_alpha_->read_sampled(alpha_input, x, y, sampler);
  output[3] = alpha_input[0];
}

void SetAlphaReplaceOperation::deinit_execution()
{
  input_color_ = nullptr;
  input_alpha_ = nullptr;
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
