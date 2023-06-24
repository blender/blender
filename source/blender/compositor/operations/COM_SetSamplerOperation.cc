/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetSamplerOperation.h"

namespace blender::compositor {

SetSamplerOperation::SetSamplerOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void SetSamplerOperation::init_execution()
{
  reader_ = this->get_input_socket_reader(0);
}
void SetSamplerOperation::deinit_execution()
{
  reader_ = nullptr;
}

void SetSamplerOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler /*sampler*/)
{
  reader_->read_sampled(output, x, y, sampler_);
}

}  // namespace blender::compositor
