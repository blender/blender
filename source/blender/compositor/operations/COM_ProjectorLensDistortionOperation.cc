/* SPDX-FileCopyrightText: 2011 Blender Foundation.
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
  flags_.complex = true;
  input_program_ = nullptr;
  dispersion_available_ = false;
  dispersion_ = 0.0f;
}

void ProjectorLensDistortionOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    NodeOperation *dispersion_input = get_input_operation(1);
    if (dispersion_input->get_flags().is_constant_operation) {
      dispersion_ = static_cast<ConstantOperation *>(dispersion_input)->get_constant_elem()[0];
    }
    kr_ = 0.25f * max_ff(min_ff(dispersion_, 1.0f), 0.0f);
    kr2_ = kr_ * 20;
  }
}

void ProjectorLensDistortionOperation::init_execution()
{
  this->init_mutex();
  input_program_ = this->get_input_socket_reader(0);
}

void *ProjectorLensDistortionOperation::initialize_tile_data(rcti * /*rect*/)
{
  update_dispersion();
  void *buffer = input_program_->initialize_tile_data(nullptr);
  return buffer;
}

void ProjectorLensDistortionOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float input_value[4];
  const float height = this->get_height();
  const float width = this->get_width();
  const float v = (y + 0.5f) / height;
  const float u = (x + 0.5f) / width;
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  input_buffer->read_bilinear(input_value, (u * width + kr2_) - 0.5f, v * height - 0.5f);
  output[0] = input_value[0];
  input_buffer->read(input_value, x, y);
  output[1] = input_value[1];
  input_buffer->read_bilinear(input_value, (u * width - kr2_) - 0.5f, v * height - 0.5f);
  output[2] = input_value[2];
  output[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinit_execution()
{
  this->deinit_mutex();
  input_program_ = nullptr;
}

bool ProjectorLensDistortionOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  if (dispersion_available_) {
    new_input.ymax = input->ymax;
    new_input.ymin = input->ymin;
    new_input.xmin = input->xmin - kr2_ - 2;
    new_input.xmax = input->xmax + kr2_ + 2;
  }
  else {
    rcti disp_input;
    BLI_rcti_init(&disp_input, 0, 5, 0, 5);
    if (this->get_input_operation(1)->determine_depending_area_of_interest(
            &disp_input, read_operation, output))
    {
      return true;
    }
    new_input.xmin = input->xmin - 7; /* (0.25f * 20 * 1) + 2 == worse case dispersion */
    new_input.ymin = input->ymin;
    new_input.ymax = input->ymax;
    new_input.xmax = input->xmax + 7; /* (0.25f * 20 * 1) + 2 == worse case dispersion */
  }
  if (this->get_input_operation(0)->determine_depending_area_of_interest(
          &new_input, read_operation, output))
  {
    return true;
  }
  return false;
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void ProjectorLensDistortionOperation::update_dispersion()
{
  if (dispersion_available_) {
    return;
  }
  this->lock_mutex();
  if (!dispersion_available_) {
    float result[4];
    this->get_input_socket_reader(1)->read_sampled(result, 1, 1, PixelSampler::Nearest);
    dispersion_ = result[0];
    kr_ = 0.25f * max_ff(min_ff(dispersion_, 1.0f), 0.0f);
    kr2_ = kr_ * 20;
    dispersion_available_ = true;
  }
  this->unlock_mutex();
}

void ProjectorLensDistortionOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  switch (execution_model_) {
    case eExecutionModel::FullFrame: {
      set_determined_canvas_modifier([=](rcti &canvas) {
        /* Ensure screen space. */
        BLI_rcti_translate(&canvas, -canvas.xmin, -canvas.ymin);
      });
      break;
    }
    default:
      break;
  }

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
