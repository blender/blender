/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MapValueOperation.h"

namespace blender::compositor {

MapValueOperation::MapValueOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_operation_ = nullptr;
  flags_.can_be_constant = true;
}

void MapValueOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void MapValueOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float src[4];
  input_operation_->read_sampled(src, x, y, sampler);
  const TexMapping *texmap = settings_;
  float value = (src[0] + texmap->loc[0]) * texmap->size[0];
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

  output[0] = value;
}

void MapValueOperation::deinit_execution()
{
  input_operation_ = nullptr;
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
