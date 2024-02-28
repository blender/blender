/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SetAlphaReplaceOperation.h"

namespace blender::compositor {

SetAlphaReplaceOperation::SetAlphaReplaceOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  flags_.can_be_constant = true;
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
