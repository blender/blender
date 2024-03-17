/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareBaseOperation.h"

namespace blender::compositor {

GlareBaseOperation::GlareBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  settings_ = nullptr;
  flags_.can_be_constant = true;
  is_output_rendered_ = false;
}

void GlareBaseOperation::get_area_of_interest(const int input_idx,
                                              const rcti & /*output_area*/,
                                              rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = 0;
  r_input_area.xmax = this->get_width();
  r_input_area.ymin = 0;
  r_input_area.ymax = this->get_height();
}

void GlareBaseOperation::update_memory_buffer(MemoryBuffer *output,
                                              const rcti & /*area*/,
                                              Span<MemoryBuffer *> inputs)
{
  if (!is_output_rendered_) {
    MemoryBuffer *input = inputs[0];
    if (input->is_a_single_elem()) {
      copy_v4_v4(output->get_elem(0, 0), input->get_elem(0, 0));
      is_output_rendered_ = true;
      return;
    }

    this->generate_glare(output->get_buffer(), input, settings_);
    is_output_rendered_ = true;
  }
}

}  // namespace blender::compositor
