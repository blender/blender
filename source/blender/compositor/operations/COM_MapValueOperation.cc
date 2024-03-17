/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MapValueOperation.h"

namespace blender::compositor {

MapValueOperation::MapValueOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.can_be_constant = true;
}

void MapValueOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float input = *it.in(0);
    const TexMapping *texmap = settings_;
    float value = (input + texmap->loc[0]) * texmap->size[0];
    if (texmap->flag & TEXMAP_CLIP_MIN) {
      if (value < texmap->min[0]) {
        value = texmap->min[0];
      }
    }
    if (texmap->flag & TEXMAP_CLIP_MAX) {
      if (value > texmap->max[0]) {
        value = texmap->max[0];
      }
    }

    it.out[0] = value;
  }
}

}  // namespace blender::compositor
