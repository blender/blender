/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_FlipOperation.h"

namespace blender::compositor {

FlipOperation::FlipOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::None);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  flip_x_ = true;
  flip_y_ = false;
  flags_.can_be_constant = true;
}

void FlipOperation::get_area_of_interest(const int input_idx,
                                         const rcti & /*output_area*/,
                                         rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);

  /* The full input image should be flipped to avoid cropping effects caused by previous scaling or
   * translating, or a smaller output area. */
  r_input_area = get_canvas();
}

void FlipOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                 const rcti &area,
                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  if (input_img->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input_img->get_elem(0, 0));
    return;
  }
  const int input_offset_x = input_img->get_rect().xmin;
  const int input_offset_y = input_img->get_rect().ymin;
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const int nx = flip_x_ ? (int(this->get_width()) - 1) - it.x : it.x;
    const int ny = flip_y_ ? (int(this->get_height()) - 1) - it.y : it.y;
    input_img->read_elem(input_offset_x + nx, input_offset_y + ny, it.out);
  }
}

}  // namespace blender::compositor
