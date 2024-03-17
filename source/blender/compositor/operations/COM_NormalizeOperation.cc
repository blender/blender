/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_NormalizeOperation.h"

namespace blender::compositor {

NormalizeOperation::NormalizeOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  cached_instance_ = nullptr;
  flags_.can_be_constant = true;
}

void NormalizeOperation::deinit_execution()
{
  delete cached_instance_;
  cached_instance_ = nullptr;
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel
 */
#define BLENDER_ZMAX 10000.0f

void NormalizeOperation::get_area_of_interest(const int /*input_idx*/,
                                              const rcti & /*output_area*/,
                                              rcti &r_input_area)
{
  r_input_area = get_input_operation(0)->get_canvas();
}

void NormalizeOperation::update_memory_buffer_started(MemoryBuffer * /*output*/,
                                                      const rcti & /*area*/,
                                                      Span<MemoryBuffer *> inputs)
{
  if (cached_instance_ == nullptr) {
    MemoryBuffer *input = inputs[0];

    /* Using generic two floats struct to store `x: min`, `y: multiply`. */
    NodeTwoFloats *minmult = new NodeTwoFloats();

    float minv = 1.0f + BLENDER_ZMAX;
    float maxv = -1.0f - BLENDER_ZMAX;
    for (const float *elem : input->as_range()) {
      const float value = *elem;
      if ((value > maxv) && (value <= BLENDER_ZMAX)) {
        maxv = value;
      }
      if ((value < minv) && (value >= -BLENDER_ZMAX)) {
        minv = value;
      }
    }

    minmult->x = minv;
    /* The case of a flat buffer would cause a divide by 0. */
    minmult->y = ((maxv != minv) ? 1.0f / (maxv - minv) : 0.0f);

    cached_instance_ = minmult;
  }
}

void NormalizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  NodeTwoFloats *minmult = cached_instance_;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float input_value = *it.in(0);

    *it.out = (input_value - minmult->x) * minmult->y;

    /* Clamp infinities. */
    CLAMP(*it.out, 0.0f, 1.0f);
  }
}

}  // namespace blender::compositor
