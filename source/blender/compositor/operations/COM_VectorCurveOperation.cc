/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_VectorCurveOperation.h"

#include "BKE_colortools.hh"

namespace blender::compositor {

VectorCurveOperation::VectorCurveOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Vector);

  this->flags_.can_be_constant = true;
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
