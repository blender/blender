/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorSpillOperation.h"
#define AVG(a, b) ((a + b) / 2)

namespace blender::compositor {

ColorSpillOperation::ColorSpillOperation()
{
  add_input_socket(DataType::Color);
  add_input_socket(DataType::Value);
  add_output_socket(DataType::Color);

  spill_channel_ = 1; /* GREEN */
  spill_method_ = 0;
  flags_.can_be_constant = true;
}

void ColorSpillOperation::init_execution()
{
  if (spill_channel_ == 0) {
    rmut_ = -1.0f;
    gmut_ = 1.0f;
    bmut_ = 1.0f;
    channel2_ = 1;
    channel3_ = 2;
    if (settings_->unspill == 0) {
      settings_->uspillr = 1.0f;
      settings_->uspillg = 0.0f;
      settings_->uspillb = 0.0f;
    }
  }
  else if (spill_channel_ == 1) {
    rmut_ = 1.0f;
    gmut_ = -1.0f;
    bmut_ = 1.0f;
    channel2_ = 0;
    channel3_ = 2;
    if (settings_->unspill == 0) {
      settings_->uspillr = 0.0f;
      settings_->uspillg = 1.0f;
      settings_->uspillb = 0.0f;
    }
  }
  else {
    rmut_ = 1.0f;
    gmut_ = 1.0f;
    bmut_ = -1.0f;

    channel2_ = 0;
    channel3_ = 1;
    if (settings_->unspill == 0) {
      settings_->uspillr = 0.0f;
      settings_->uspillg = 0.0f;
      settings_->uspillb = 1.0f;
    }
  }
}

void ColorSpillOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float factor = std::min(1.0f, *it.in(1));

    float map;
    switch (spill_method_) {
      case 0: /* simple */
        map = factor * (color[spill_channel_] - (settings_->limscale * color[settings_->limchan]));
        break;
      default: /* average */
        map = factor * (color[spill_channel_] -
                        (settings_->limscale * AVG(color[channel2_], color[channel3_])));
        break;
    }

    if (map > 0.0f) {
      it.out[0] = color[0] + rmut_ * (settings_->uspillr * map);
      it.out[1] = color[1] + gmut_ * (settings_->uspillg * map);
      it.out[2] = color[2] + bmut_ * (settings_->uspillb * map);
      it.out[3] = color[3];
    }
    else {
      copy_v4_v4(it.out, color);
    }
  }
}

}  // namespace blender::compositor
