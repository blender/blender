/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorRampOperation.h"

#include "BKE_colorband.hh"

namespace blender::compositor {

ColorRampOperation::ColorRampOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  color_band_ = nullptr;
  flags_.can_be_constant = true;
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
