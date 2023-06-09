/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
