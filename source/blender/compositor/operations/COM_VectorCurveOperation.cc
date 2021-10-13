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

#include "COM_VectorCurveOperation.h"

#include "BKE_colortools.h"

namespace blender::compositor {

VectorCurveOperation::VectorCurveOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Vector);

  input_program_ = nullptr;
}
void VectorCurveOperation::init_execution()
{
  CurveBaseOperation::init_execution();
  input_program_ = this->get_input_socket_reader(0);
}

void VectorCurveOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input[4];

  input_program_->read_sampled(input, x, y, sampler);

  BKE_curvemapping_evaluate_premulRGBF(curve_mapping_, output, input);
}

void VectorCurveOperation::deinit_execution()
{
  CurveBaseOperation::deinit_execution();
  input_program_ = nullptr;
}

void VectorCurveOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  CurveMapping *curve_map = curve_mapping_;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    BKE_curvemapping_evaluate_premulRGBF(curve_map, it.out, it.in(0));
  }
}

}  // namespace blender::compositor
