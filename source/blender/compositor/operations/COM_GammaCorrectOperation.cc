/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GammaCorrectOperation.h"

namespace blender::compositor {

GammaCorrectOperation::GammaCorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void GammaCorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    /* Check for negative to avoid NAN's. */
    it.out[0] = color[0] > 0.0f ? color[0] * color[0] : 0.0f;
    it.out[1] = color[1] > 0.0f ? color[1] * color[1] : 0.0f;
    it.out[2] = color[2] > 0.0f ? color[2] * color[2] : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

GammaUncorrectOperation::GammaUncorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void GammaUncorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    it.out[0] = color[0] > 0.0f ? sqrtf(color[0]) : 0.0f;
    it.out[1] = color[1] > 0.0f ? sqrtf(color[1]) : 0.0f;
    it.out[2] = color[2] > 0.0f ? sqrtf(color[2]) : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

}  // namespace blender::compositor
