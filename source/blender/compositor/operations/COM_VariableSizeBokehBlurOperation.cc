/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "COM_OpenCLDevice.h"
#include "COM_VariableSizeBokehBlurOperation.h"

namespace blender::compositor {

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color, ResizeMode::Align); /* Do not resize the bokeh image. */
  this->add_input_socket(DataType::Value);                    /* Radius. */
  this->add_input_socket(DataType::Value);                    /* Bounding Box. */
#ifdef COM_DEFOCUS_SEARCH
  /* Inverse search radius optimization structure. */
  this->add_input_socket(DataType::Color, ResizeMode::None);
#endif
  this->add_output_socket(DataType::Color);
  flags_.complex = true;
  flags_.open_cl = true;
  flags_.can_be_constant = true;

  input_program_ = nullptr;
  input_bokeh_program_ = nullptr;
  input_size_program_ = nullptr;
  input_mask_program_ = nullptr;
  max_blur_ = 32.0f;
  threshold_ = 1.0f;
  do_size_scale_ = false;
#ifdef COM_DEFOCUS_SEARCH
  input_search_program_ = nullptr;
#endif
}

void VariableSizeBokehBlurOperation::init_execution()
{
  input_program_ = get_input_socket_reader(0);
  input_bokeh_program_ = get_input_socket_reader(1);
  input_size_program_ = get_input_socket_reader(2);
  input_mask_program_ = get_input_socket_reader(3);
#ifdef COM_DEFOCUS_SEARCH
  input_search_program_ = get_input_socket_reader(4);
#endif
  QualityStepHelper::init_execution(COM_QH_INCREASE);
}
struct VariableSizeBokehBlurTileData {
  MemoryBuffer *color;
  MemoryBuffer *bokeh;
  MemoryBuffer *size;
  MemoryBuffer *mask;
  int max_blur_scalar;
};

void *VariableSizeBokehBlurOperation::initialize_tile_data(rcti *rect)
{
  VariableSizeBokehBlurTileData *data = new VariableSizeBokehBlurTileData();
  data->color = (MemoryBuffer *)input_program_->initialize_tile_data(rect);
  data->bokeh = (MemoryBuffer *)input_bokeh_program_->initialize_tile_data(rect);
  data->size = (MemoryBuffer *)input_size_program_->initialize_tile_data(rect);
  data->mask = (MemoryBuffer *)input_mask_program_->initialize_tile_data(rect);

  rcti rect2 = COM_AREA_NONE;
  this->determine_depending_area_of_interest(
      rect, (ReadBufferOperation *)input_size_program_, &rect2);

  const float max_dim = std::max(this->get_width(), this->get_height());
  const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;

  data->max_blur_scalar = int(data->size->get_max_value(rect2) * scalar);
  CLAMP(data->max_blur_scalar, 0, max_blur_);
  return data;
}

void VariableSizeBokehBlurOperation::deinitialize_tile_data(rcti * /*rect*/, void *data)
{
  VariableSizeBokehBlurTileData *result = (VariableSizeBokehBlurTileData *)data;
  delete result;
}

void VariableSizeBokehBlurOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  VariableSizeBokehBlurTileData *tile_data = (VariableSizeBokehBlurTileData *)data;
  MemoryBuffer *input_buffer = tile_data->color;
  MemoryBuffer *bokeh_buffer = tile_data->bokeh;
  MemoryBuffer *size_buffer = tile_data->size;
  MemoryBuffer *mask_buffer = tile_data->mask;

  if (*mask_buffer->get_elem(x, y) <= 0.0f) {
    copy_v4_v4(output, input_buffer->get_elem(x, y));
    return;
  }

  const float max_dim = std::max(get_width(), get_height());
  const float base_size = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  const int search_radius = tile_data->max_blur_scalar;
  const int2 bokeh_size = int2(bokeh_buffer->get_width(), bokeh_buffer->get_height());
  const float center_size = math::max(0.0f, *size_buffer->get_elem(x, y) * base_size);

  float4 accumulated_color = float4(input_buffer->get_elem(x, y));
  float4 accumulated_weight = float4(1.0f);
  const int step = get_step();
  if (center_size >= threshold_) {
    for (int yi = -search_radius; yi <= search_radius; yi += step) {
      for (int xi = -search_radius; xi <= search_radius; xi += step) {
        if (xi == 0 && yi == 0) {
          continue;
        }
        const float candidate_size = math::max(
            0.0f, *size_buffer->get_elem_clamped(x + xi, y + yi) * base_size);
        const float size = math::min(center_size, candidate_size);
        if (size < threshold_ || math::max(math::abs(xi), math::abs(yi)) > size) {
          continue;
        }

        const float2 normalized_texel = (float2(xi, yi) + size + 0.5f) / (size * 2.0f + 1.0f);
        const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
        const float4 weight = bokeh_buffer->get_elem(int(weight_texel.x), int(weight_texel.y));
        const float4 color = input_buffer->get_elem_clamped(x + xi, y + yi);
        accumulated_color += color * weight;
        accumulated_weight += weight;
      }
    }
  }

  const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
  copy_v4_v4(output, final_color);

  /* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
  if ((center_size > threshold_) && (center_size < threshold_ * 2.0f)) {
    /* factor from 0-1 */
    float fac = (center_size - threshold_) / threshold_;
    interp_v4_v4v4(output, input_buffer->get_elem(x, y), output, fac);
  }
}

void VariableSizeBokehBlurOperation::execute_opencl(
    OpenCLDevice *device,
    MemoryBuffer *output_memory_buffer,
    cl_mem cl_output_buffer,
    MemoryBuffer **input_memory_buffers,
    std::list<cl_mem> *cl_mem_to_clean_up,
    std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel defocus_kernel = device->COM_cl_create_kernel("defocus_kernel", nullptr);

  cl_int step = this->get_step();
  cl_int max_blur;
  cl_float threshold = threshold_;

  MemoryBuffer *size_memory_buffer = input_size_program_->get_input_memory_buffer(
      input_memory_buffers);

  const float max_dim = std::max(get_width(), get_height());
  cl_float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;

  max_blur = (cl_int)min_ff(size_memory_buffer->get_max_value() * scalar, float(max_blur_));

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      defocus_kernel, 0, -1, cl_mem_to_clean_up, input_memory_buffers, input_program_);
  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      defocus_kernel, 1, -1, cl_mem_to_clean_up, input_memory_buffers, input_bokeh_program_);
  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      defocus_kernel, 2, 4, cl_mem_to_clean_up, input_memory_buffers, input_size_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(
      defocus_kernel, 3, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
      defocus_kernel, 5, output_memory_buffer);
  clSetKernelArg(defocus_kernel, 6, sizeof(cl_int), &step);
  clSetKernelArg(defocus_kernel, 7, sizeof(cl_int), &max_blur);
  clSetKernelArg(defocus_kernel, 8, sizeof(cl_float), &threshold);
  clSetKernelArg(defocus_kernel, 9, sizeof(cl_float), &scalar);
  device->COM_cl_attach_size_to_kernel_parameter(defocus_kernel, 10, this);

  device->COM_cl_enqueue_range(defocus_kernel, output_memory_buffer, 11, this);
}

void VariableSizeBokehBlurOperation::deinit_execution()
{
  input_program_ = nullptr;
  input_bokeh_program_ = nullptr;
  input_size_program_ = nullptr;
  input_mask_program_ = nullptr;
#ifdef COM_DEFOCUS_SEARCH
  input_search_program_ = nullptr;
#endif
}

bool VariableSizeBokehBlurOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  if (read_operation == (ReadBufferOperation *)get_input_operation(BOKEH_INPUT_INDEX)) {
    rcti bokeh_input;
    bokeh_input.xmax = COM_BLUR_BOKEH_PIXELS;
    bokeh_input.xmin = 0;
    bokeh_input.ymax = COM_BLUR_BOKEH_PIXELS;
    bokeh_input.ymin = 0;

    NodeOperation *operation = get_input_operation(BOKEH_INPUT_INDEX);
    return operation->determine_depending_area_of_interest(&bokeh_input, read_operation, output);
  }

  const float max_dim = std::max(get_width(), get_height());
  const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  int max_blur_scalar = max_blur_ * scalar;

  rcti new_input;
  new_input.xmax = input->xmax + max_blur_scalar + 2;
  new_input.xmin = input->xmin - max_blur_scalar + 2;
  new_input.ymax = input->ymax + max_blur_scalar - 2;
  new_input.ymin = input->ymin - max_blur_scalar - 2;

  NodeOperation *operation = get_input_operation(SIZE_INPUT_INDEX);
  if (operation->determine_depending_area_of_interest(&new_input, read_operation, output)) {
    return true;
  }
#ifdef COM_DEFOCUS_SEARCH
  rcti search_input;
  search_input.xmax = (input->xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
  search_input.xmin = (input->xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
  search_input.ymax = (input->ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
  search_input.ymin = (input->ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
  operation = get_input_operation(DEFOCUS_INPUT_INDEX);
  if (operation->determine_depending_area_of_interest(&search_input, read_operation, output)) {
    return true;
  }
#endif
  operation = get_input_operation(IMAGE_INPUT_INDEX);
  if (operation->determine_depending_area_of_interest(&new_input, read_operation, output)) {
    return true;
  }

  operation = get_input_operation(BOUNDING_BOX_INPUT_INDEX);
  if (operation->determine_depending_area_of_interest(&new_input, read_operation, output)) {
    return true;
  }

  return false;
}

void VariableSizeBokehBlurOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX:
    case BOUNDING_BOX_INPUT_INDEX:
    case SIZE_INPUT_INDEX: {
      const float max_dim = std::max(get_width(), get_height());
      const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
      const int max_blur_scalar = max_blur_ * scalar;
      r_input_area.xmax = output_area.xmax + max_blur_scalar + 2;
      r_input_area.xmin = output_area.xmin - max_blur_scalar - 2;
      r_input_area.ymax = output_area.ymax + max_blur_scalar + 2;
      r_input_area.ymin = output_area.ymin - max_blur_scalar - 2;
      break;
    }
    case BOKEH_INPUT_INDEX: {
      r_input_area = output_area;
      r_input_area.xmax = r_input_area.xmin + COM_BLUR_BOKEH_PIXELS;
      r_input_area.ymax = r_input_area.ymin + COM_BLUR_BOKEH_PIXELS;
      break;
    }
#ifdef COM_DEFOCUS_SEARCH
    case DEFOCUS_INPUT_INDEX: {
      r_input_area.xmax = (output_area.xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
      r_input_area.xmin = (output_area.xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
      r_input_area.ymax = (output_area.ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
      r_input_area.ymin = (output_area.ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
      break;
    }
#endif
  }
}

void VariableSizeBokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input_buffer = inputs[0];
  MemoryBuffer *bokeh_buffer = inputs[1];
  MemoryBuffer *size_buffer = inputs[2];
  MemoryBuffer *mask_buffer = inputs[3];

  const float max_dim = std::max(get_width(), get_height());
  const float base_size = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  const float maximum_size = size_buffer->get_max_value();
  const int search_radius = math::clamp(int(maximum_size * base_size), 0, max_blur_);
  const int2 bokeh_size = int2(bokeh_buffer->get_width(), bokeh_buffer->get_height());

  BuffersIterator<float> it = output->iterate_with({}, area);
  for (; !it.is_end(); ++it) {
    if (*mask_buffer->get_elem(it.x, it.y) <= 0.0f) {
      copy_v4_v4(it.out, input_buffer->get_elem(it.x, it.y));
      continue;
    }

    const float center_size = math::max(0.0f, *size_buffer->get_elem(it.x, it.y) * base_size);

    float4 accumulated_color = float4(input_buffer->get_elem(it.x, it.y));
    float4 accumulated_weight = float4(1.0f);
    const int step = get_step();
    if (center_size >= threshold_) {
      for (int yi = -search_radius; yi <= search_radius; yi += step) {
        for (int xi = -search_radius; xi <= search_radius; xi += step) {
          if (xi == 0 && yi == 0) {
            continue;
          }
          const float candidate_size = math::max(
              0.0f, *size_buffer->get_elem_clamped(it.x + xi, it.y + yi) * base_size);
          const float size = math::min(center_size, candidate_size);
          if (size < threshold_ || math::max(math::abs(xi), math::abs(yi)) > size) {
            continue;
          }

          const float2 normalized_texel = (float2(xi, yi) + size + 0.5f) / (size * 2.0f + 1.0f);
          const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
          const float4 weight = bokeh_buffer->get_elem(int(weight_texel.x), int(weight_texel.y));
          const float4 color = input_buffer->get_elem_clamped(it.x + xi, it.y + yi);
          accumulated_color += color * weight;
          accumulated_weight += weight;
        }
      }
    }

    const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
    copy_v4_v4(it.out, final_color);

    /* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
    if ((center_size > threshold_) && (center_size < threshold_ * 2.0f)) {
      /* factor from 0-1 */
      float fac = (center_size - threshold_) / threshold_;
      interp_v4_v4v4(it.out, input_buffer->get_elem(it.x, it.y), it.out, fac);
    }
  }
}

#ifdef COM_DEFOCUS_SEARCH
/* #InverseSearchRadiusOperation. */
InverseSearchRadiusOperation::InverseSearchRadiusOperation()
{
  this->add_input_socket(DataType::Value, ResizeMode::Align); /* Radius. */
  this->add_output_socket(DataType::Color);
  this->flags.complex = true;
  input_radius_ = nullptr;
}

void InverseSearchRadiusOperation::init_execution()
{
  input_radius_ = this->get_input_socket_reader(0);
}

void *InverseSearchRadiusOperation::initialize_tile_data(rcti *rect)
{
  MemoryBuffer *data = new MemoryBuffer(DataType::Color, rect);
  float *buffer = data->get_buffer();
  int x, y;
  int width = input_radius_->get_width();
  int height = input_radius_->get_height();
  float temp[4];
  int offset = 0;
  for (y = rect->ymin; y < rect->ymax; y++) {
    for (x = rect->xmin; x < rect->xmax; x++) {
      int rx = x * DIVIDER;
      int ry = y * DIVIDER;
      buffer[offset] = std::max(rx - max_blur_, 0);
      buffer[offset + 1] = std::max(ry - max_blur_, 0);
      buffer[offset + 2] = std::min(rx + DIVIDER + max_blur_, width);
      buffer[offset + 3] = std::min(ry + DIVIDER + max_blur_, height);
      offset += 4;
    }
  }
#  if 0
  for (x = rect->xmin; x < rect->xmax; x++) {
    for (y = rect->ymin; y < rect->ymax; y++) {
      int rx = x * DIVIDER;
      int ry = y * DIVIDER;
      float radius = 0.0f;
      float maxx = x;
      float maxy = y;

      for (int x2 = 0; x2 < DIVIDER; x2++) {
        for (int y2 = 0; y2 < DIVIDER; y2++) {
          input_radius_->read(temp, rx + x2, ry + y2, PixelSampler::Nearest);
          if (radius < temp[0]) {
            radius = temp[0];
            maxx = x2;
            maxy = y2;
          }
        }
      }
      int impact_radius = ceil(radius / DIVIDER);
      for (int x2 = x - impact_radius; x2 < x + impact_radius; x2++) {
        for (int y2 = y - impact_radius; y2 < y + impact_radius; y2++) {
          data->read(temp, x2, y2);
          temp[0] = std::min(temp[0], maxx);
          temp[1] = std::min(temp[1], maxy);
          temp[2] = std::max(temp[2], maxx);
          temp[3] = std::max(temp[3], maxy);
          data->write_pixel(x2, y2, temp);
        }
      }
    }
  }
#  endif
  return data;
}

void InverseSearchRadiusOperation::execute_pixel_chunk(float output[4], int x, int y, void *data)
{
  MemoryBuffer *buffer = (MemoryBuffer *)data;
  buffer->read_no_check(output, x, y);
}

void InverseSearchRadiusOperation::deinitialize_tile_data(rcti *rect, void *data)
{
  if (data) {
    MemoryBuffer *mb = (MemoryBuffer *)data;
    delete mb;
  }
}

void InverseSearchRadiusOperation::deinit_execution()
{
  input_radius_ = nullptr;
}

void InverseSearchRadiusOperation::determine_resolution(uint resolution[2],
                                                        uint preferred_resolution[2])
{
  NodeOperation::determine_resolution(resolution, preferred_resolution);
  resolution[0] = resolution[0] / DIVIDER;
  resolution[1] = resolution[1] / DIVIDER;
}

bool InverseSearchRadiusOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_rect;
  new_rect.ymin = input->ymin * DIVIDER - max_blur_;
  new_rect.ymax = input->ymax * DIVIDER + max_blur_;
  new_rect.xmin = input->xmin * DIVIDER - max_blur_;
  new_rect.xmax = input->xmax * DIVIDER + max_blur_;
  return NodeOperation::determine_depending_area_of_interest(&new_rect, read_operation, output);
}
#endif

}  // namespace blender::compositor
