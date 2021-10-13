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

#include "COM_ColorRampOperation.h"

#include "BKE_colorband.h"

namespace blender::compositor {

ColorRampOperation::ColorRampOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  input_program_ = nullptr;
  color_band_ = nullptr;
  flags_.can_be_constant = true;
}
void ColorRampOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void ColorRampOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float values[4];

  input_program_->read_sampled(values, x, y, sampler);
  BKE_colorband_evaluate(color_band_, values[0], output);
}

void ColorRampOperation::deinit_execution()
{
  input_program_ = nullptr;
}

void ColorRampOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    BKE_colorband_evaluate(color_band_, *it.in(0), it.out);
  }
}

}  // namespace blender::compositor
