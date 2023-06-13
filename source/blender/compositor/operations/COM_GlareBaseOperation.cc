/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareBaseOperation.h"

namespace blender::compositor {

GlareBaseOperation::GlareBaseOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  settings_ = nullptr;
  flags_.is_fullframe_operation = true;
  is_output_rendered_ = false;
}
void GlareBaseOperation::init_execution()
{
  SingleThreadedOperation::init_execution();
  input_program_ = get_input_socket_reader(0);
}

void GlareBaseOperation::deinit_execution()
{
  input_program_ = nullptr;
  SingleThreadedOperation::deinit_execution();
}

MemoryBuffer *GlareBaseOperation::create_memory_buffer(rcti *rect2)
{
  MemoryBuffer *tile = (MemoryBuffer *)input_program_->initialize_tile_data(rect2);
  rcti rect;
  rect.xmin = 0;
  rect.ymin = 0;
  rect.xmax = get_width();
  rect.ymax = get_height();
  MemoryBuffer *result = new MemoryBuffer(DataType::Color, rect);
  float *data = result->get_buffer();
  this->generate_glare(data, tile, settings_);
  return result;
}

bool GlareBaseOperation::determine_depending_area_of_interest(rcti * /*input*/,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  if (is_cached()) {
    return false;
  }

  rcti new_input;
  new_input.xmax = this->get_width();
  new_input.xmin = 0;
  new_input.ymax = this->get_height();
  new_input.ymin = 0;
  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
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
    const bool is_input_inflated = input->is_a_single_elem();
    if (is_input_inflated) {
      input = input->inflate();
    }

    this->generate_glare(output->get_buffer(), input, settings_);
    is_output_rendered_ = true;

    if (is_input_inflated) {
      delete input;
    }
  }
}

}  // namespace blender::compositor
