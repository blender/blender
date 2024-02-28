/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ZCombineOperation.h"

namespace blender::compositor {

ZCombineOperation::ZCombineOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  flags_.can_be_constant = true;
}

void ZCombineOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                     const rcti &area,
                                                     Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float depth1 = *it.in(1);
    const float depth2 = *it.in(3);
    const float *color = (depth1 < depth2) ? it.in(0) : it.in(2);
    copy_v4_v4(it.out, color);
  }
}

void ZCombineAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float depth1 = *it.in(1);
    const float depth2 = *it.in(3);
    const float *color1;
    const float *color2;
    if (depth1 < depth2) {
      color1 = it.in(0);
      color2 = it.in(2);
    }
    else {
      color1 = it.in(2);
      color2 = it.in(0);
    }
    const float fac = color1[3];
    const float ifac = 1.0f - fac;
    it.out[0] = fac * color1[0] + ifac * color2[0];
    it.out[1] = fac * color1[1] + ifac * color2[1];
    it.out[2] = fac * color1[2] + ifac * color2[2];
    it.out[3] = std::max(color1[3], color2[3]);
  }
}

// MASK combine
ZCombineMaskOperation::ZCombineMaskOperation()
{
  this->add_input_socket(DataType::Value);  // mask
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
}

void ZCombineMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float mask = *it.in(0);
    const float *color1 = it.in(1);
    const float *color2 = it.in(2);
    interp_v4_v4v4(it.out, color1, color2, 1.0f - mask);
  }
}

void ZCombineMaskAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                              const rcti &area,
                                                              Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float mask = *it.in(0);
    const float *color1 = it.in(1);
    const float *color2 = it.in(2);
    const float fac = (1.0f - mask) * (1.0f - color1[3]) + mask * color2[3];
    const float mfac = 1.0f - fac;

    it.out[0] = color1[0] * mfac + color2[0] * fac;
    it.out[1] = color1[1] * mfac + color2[1] * fac;
    it.out[2] = color1[2] * mfac + color2[2] * fac;
    it.out[3] = std::max(color1[3], color2[3]);
  }
}

}  // namespace blender::compositor
