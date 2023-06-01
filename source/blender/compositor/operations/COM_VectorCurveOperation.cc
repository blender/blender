/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
