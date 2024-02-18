/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "COM_BokehBlurOperation.h"
#include "COM_ConstantOperation.h"

#include "COM_OpenCLDevice.h"

namespace blender::compositor {

constexpr int IMAGE_INPUT_INDEX = 0;
constexpr int BOKEH_INPUT_INDEX = 1;
constexpr int BOUNDING_BOX_INPUT_INDEX = 2;
constexpr int SIZE_INPUT_INDEX = 3;

BokehBlurOperation::BokehBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  flags_.complex = true;
  flags_.open_cl = true;
  flags_.can_be_constant = true;

  size_ = 1.0f;
  sizeavailable_ = false;
  input_program_ = nullptr;
  input_bokeh_program_ = nullptr;
  input_bounding_box_reader_ = nullptr;

  extend_bounds_ = false;
}

void BokehBlurOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    update_size();
  }
}

void *BokehBlurOperation::initialize_tile_data(rcti * /*rect*/)
{
  lock_mutex();
  if (!sizeavailable_) {
    update_size();
  }
  void *buffer = get_input_operation(0)->initialize_tile_data(nullptr);
  unlock_mutex();
  return buffer;
}

void BokehBlurOperation::init_execution()
{
  init_mutex();

  input_program_ = get_input_socket_reader(0);
  input_bokeh_program_ = get_input_socket_reader(1);
  input_bounding_box_reader_ = get_input_socket_reader(2);

  QualityStepHelper::init_execution(COM_QH_INCREASE);
}

void BokehBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;

  float temp_bounding_box[4];
  input_bounding_box_reader_->read_sampled(temp_bounding_box, x, y, PixelSampler::Nearest);
  if (temp_bounding_box[0] <= 0.0f) {
    copy_v4_v4(output, input_buffer->get_elem(x, y));
    return;
  }

  const float max_dim = std::max(this->get_width(), this->get_height());
  int radius = size_ * max_dim / 100.0f;
  const int2 bokeh_size = int2(input_bokeh_program_->get_width(),
                               input_bokeh_program_->get_height());

  float4 accumulated_color = float4(0.0f);
  float4 accumulated_weight = float4(0.0f);
  int step = get_step();
  for (int yi = -radius; yi <= radius; yi += step) {
    for (int xi = -radius; xi <= radius; xi += step) {
      const float2 normalized_texel = (float2(xi, yi) + radius + 0.5f) / (radius * 2.0f + 1.0f);
      const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
      float4 weight;
      input_bokeh_program_->read(weight, int(weight_texel.x), int(weight_texel.y), nullptr);
      const float4 color = float4(input_buffer->get_elem_clamped(x + xi, y + yi)) * weight;
      accumulated_color += color;
      accumulated_weight += weight;
    }
  }

  const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
  copy_v4_v4(output, final_color);
}

void BokehBlurOperation::deinit_execution()
{
  deinit_mutex();
  input_program_ = nullptr;
  input_bokeh_program_ = nullptr;
  input_bounding_box_reader_ = nullptr;
}

bool BokehBlurOperation::determine_depending_area_of_interest(rcti *input,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti new_input;
  rcti bokeh_input;
  const float max_dim = std::max(this->get_width(), this->get_height());

  if (sizeavailable_) {
    new_input.xmax = input->xmax + (size_ * max_dim / 100.0f);
    new_input.xmin = input->xmin - (size_ * max_dim / 100.0f);
    new_input.ymax = input->ymax + (size_ * max_dim / 100.0f);
    new_input.ymin = input->ymin - (size_ * max_dim / 100.0f);
  }
  else {
    new_input.xmax = input->xmax + (10.0f * max_dim / 100.0f);
    new_input.xmin = input->xmin - (10.0f * max_dim / 100.0f);
    new_input.ymax = input->ymax + (10.0f * max_dim / 100.0f);
    new_input.ymin = input->ymin - (10.0f * max_dim / 100.0f);
  }

  NodeOperation *operation = get_input_operation(1);
  bokeh_input.xmax = operation->get_width();
  bokeh_input.xmin = 0;
  bokeh_input.ymax = operation->get_height();
  bokeh_input.ymin = 0;
  if (operation->determine_depending_area_of_interest(&bokeh_input, read_operation, output)) {
    return true;
  }
  operation = get_input_operation(0);
  if (operation->determine_depending_area_of_interest(&new_input, read_operation, output)) {
    return true;
  }
  operation = get_input_operation(2);
  if (operation->determine_depending_area_of_interest(input, read_operation, output)) {
    return true;
  }
  if (!sizeavailable_) {
    rcti size_input;
    size_input.xmin = 0;
    size_input.ymin = 0;
    size_input.xmax = 5;
    size_input.ymax = 5;
    operation = get_input_operation(3);
    if (operation->determine_depending_area_of_interest(&size_input, read_operation, output)) {
      return true;
    }
  }
  return false;
}

void BokehBlurOperation::execute_opencl(OpenCLDevice *device,
                                        MemoryBuffer *output_memory_buffer,
                                        cl_mem cl_output_buffer,
                                        MemoryBuffer **input_memory_buffers,
                                        std::list<cl_mem> *cl_mem_to_clean_up,
                                        std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel kernel = device->COM_cl_create_kernel("bokeh_blur_kernel", nullptr);
  if (!sizeavailable_) {
    update_size();
  }
  const float max_dim = std::max(this->get_width(), this->get_height());
  cl_int radius = size_ * max_dim / 100.0f;
  cl_int step = this->get_step();

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      kernel, 0, -1, cl_mem_to_clean_up, input_memory_buffers, input_bounding_box_reader_);
  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      kernel, 1, 4, cl_mem_to_clean_up, input_memory_buffers, input_program_);
  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      kernel, 2, -1, cl_mem_to_clean_up, input_memory_buffers, input_bokeh_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(kernel, 3, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(kernel, 5, output_memory_buffer);
  clSetKernelArg(kernel, 6, sizeof(cl_int), &radius);
  clSetKernelArg(kernel, 7, sizeof(cl_int), &step);
  device->COM_cl_attach_size_to_kernel_parameter(kernel, 8, this);

  device->COM_cl_enqueue_range(kernel, output_memory_buffer, 9, this);
}

void BokehBlurOperation::update_size()
{
  if (sizeavailable_) {
    return;
  }

  switch (execution_model_) {
    case eExecutionModel::Tiled: {
      float result[4];
      this->get_input_socket_reader(3)->read_sampled(result, 0, 0, PixelSampler::Nearest);
      size_ = result[0];
      CLAMP(size_, 0.0f, 10.0f);
      break;
    }
    case eExecutionModel::FullFrame: {
      NodeOperation *size_input = get_input_operation(SIZE_INPUT_INDEX);
      if (size_input->get_flags().is_constant_operation) {
        size_ = *static_cast<ConstantOperation *>(size_input)->get_constant_elem();
        CLAMP(size_, 0.0f, 10.0f);
      } /* Else use default. */
      break;
    }
  }
  sizeavailable_ = true;
}

void BokehBlurOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (!extend_bounds_) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  switch (execution_model_) {
    case eExecutionModel::Tiled: {
      NodeOperation::determine_canvas(preferred_area, r_area);
      const float max_dim = std::max(BLI_rcti_size_x(&r_area), BLI_rcti_size_y(&r_area));
      float add_size = round_to_even(2 * size_ * max_dim / 100.0f);
      r_area.xmax += add_size;
      r_area.ymax += add_size;
      break;
    }
    case eExecutionModel::FullFrame: {
      set_determined_canvas_modifier([=](rcti &canvas) {
        const float max_dim = std::max(BLI_rcti_size_x(&canvas), BLI_rcti_size_y(&canvas));
        /* Rounding to even prevents image jiggling in backdrop while switching size values. */
        float add_size = round_to_even(2 * size_ * max_dim / 100.0f);
        canvas.xmax += add_size;
        canvas.ymax += add_size;
      });
      NodeOperation::determine_canvas(preferred_area, r_area);
      break;
    }
  }
}

void BokehBlurOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      const float max_dim = std::max(this->get_width(), this->get_height());
      const float add_size = size_ * max_dim / 100.0f;
      r_input_area.xmin = output_area.xmin - add_size;
      r_input_area.xmax = output_area.xmax + add_size;
      r_input_area.ymin = output_area.ymin - add_size;
      r_input_area.ymax = output_area.ymax + add_size;
      break;
    }
    case BOKEH_INPUT_INDEX: {
      NodeOperation *bokeh_input = get_input_operation(BOKEH_INPUT_INDEX);
      r_input_area = bokeh_input->get_canvas();
      break;
    }
    case BOUNDING_BOX_INPUT_INDEX:
      r_input_area = output_area;
      break;
    case SIZE_INPUT_INDEX: {
      r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
      break;
    }
  }
}

void BokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const float max_dim = std::max(this->get_width(), this->get_height());
  const int radius = size_ * max_dim / 100.0f;

  const MemoryBuffer *image_input = inputs[IMAGE_INPUT_INDEX];
  const MemoryBuffer *bokeh_input = inputs[BOKEH_INPUT_INDEX];
  const int2 bokeh_size = int2(bokeh_input->get_width(), bokeh_input->get_height());
  MemoryBuffer *bounding_input = inputs[BOUNDING_BOX_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({bounding_input}, area);
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    const float bounding_box = *it.in(0);
    if (bounding_box <= 0.0f) {
      image_input->read_elem(x, y, it.out);
      continue;
    }

    float4 accumulated_color = float4(0.0f);
    float4 accumulated_weight = float4(0.0f);
    const int step = get_step();
    for (int yi = -radius; yi <= radius; yi += step) {
      for (int xi = -radius; xi <= radius; xi += step) {
        const float2 normalized_texel = (float2(xi, yi) + radius + 0.5f) / (radius * 2.0f + 1.0f);
        const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
        const float4 weight = bokeh_input->get_elem(int(weight_texel.x), int(weight_texel.y));
        const float4 color = float4(image_input->get_elem_clamped(x + xi, y + yi)) * weight;
        accumulated_color += color;
        accumulated_weight += weight;
      }
    }

    const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
    copy_v4_v4(it.out, final_color);
  }
}

}  // namespace blender::compositor
