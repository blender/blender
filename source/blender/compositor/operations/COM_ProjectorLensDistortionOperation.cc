/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ConstantOperation.h"

namespace blender::compositor {

ProjectorLensDistortionOperation::ProjectorLensDistortionOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
  dispersion_ = 0.0f;
}

void ProjectorLensDistortionOperation::init_data()
{
  NodeOperation *dispersion_input = get_input_operation(1);
  if (dispersion_input->get_flags().is_constant_operation) {
    dispersion_ = static_cast<ConstantOperation *>(dispersion_input)->get_constant_elem()[0];
  }
  kr_ = 0.25f * max_ff(min_ff(dispersion_, 1.0f), 0.0f);
  kr2_ = kr_ * 20;
}

void ProjectorLensDistortionOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  set_determined_canvas_modifier([=](rcti &canvas) {
    /* Ensure screen space. */
    BLI_rcti_translate(&canvas, -canvas.xmin, -canvas.ymin);
  });

  NodeOperation::determine_canvas(preferred_area, r_area);
}

void ProjectorLensDistortionOperation::get_area_of_interest(const int input_idx,
                                                            const rcti &output_area,
                                                            rcti &r_input_area)
{
  if (input_idx == 1) {
    /* Dispersion input is used as constant only. */
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
    return;
  }

  r_input_area.ymax = output_area.ymax;
  r_input_area.ymin = output_area.ymin;
  r_input_area.xmin = output_area.xmin - kr2_ - 2;
  r_input_area.xmax = output_area.xmax + kr2_ + 2;
}

void ProjectorLensDistortionOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                    const rcti &area,
                                                                    Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_image = inputs[0];
  const float height = this->get_height();
  const float width = this->get_width();
  float color[4];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const float v = (it.y + 0.5f) / height;
    const float u = (it.x + 0.5f) / width;
    input_image->read_elem_bilinear((u * width + kr2_) - 0.5f, v * height - 0.5f, color);
    it.out[0] = color[0];
    input_image->read_elem(it.x, it.y, color);
    it.out[1] = color[1];
    input_image->read_elem_bilinear((u * width - kr2_) - 0.5f, v * height - 0.5f, color);
    it.out[2] = color[2];
    it.out[3] = 1.0f;
  }
}

}  // namespace blender::compositor
