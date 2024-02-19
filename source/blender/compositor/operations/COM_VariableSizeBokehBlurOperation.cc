/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color, ResizeMode::Align); /* Do not resize the bokeh image. */
  this->add_input_socket(DataType::Value);                    /* Radius. */
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
#ifdef COM_DEFOCUS_SEARCH
  input_search_program_ = get_input_socket_reader(3);
#endif
  QualityStepHelper::init_execution(COM_QH_INCREASE);
}
struct VariableSizeBokehBlurTileData {
  MemoryBuffer *color;
  MemoryBuffer *bokeh;
  MemoryBuffer *size;
  int max_blur_scalar;
};

void *VariableSizeBokehBlurOperation::initialize_tile_data(rcti *rect)
{
  VariableSizeBokehBlurTileData *data = new VariableSizeBokehBlurTileData();
  data->color = (MemoryBuffer *)input_program_->initialize_tile_data(rect);
  data->bokeh = (MemoryBuffer *)input_bokeh_program_->initialize_tile_data(rect);
  data->size = (MemoryBuffer *)input_size_program_->initialize_tile_data(rect);

  rcti rect2 = COM_AREA_NONE;
  this->determine_depending_area_of_interest(
      rect, (ReadBufferOperation *)input_size_program_, &rect2);

  const float max_dim = std::max(this->get_width(), this->get_height());
  const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;

  data->max_blur_scalar = int(data->size->get_max_value(rect2) * scalar);
  CLAMP(data->max_blur_scalar, 1.0f, max_blur_);
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
  MemoryBuffer *input_program_buffer = tile_data->color;
  MemoryBuffer *input_bokeh_buffer = tile_data->bokeh;
  MemoryBuffer *input_size_buffer = tile_data->size;
  float *input_size_float_buffer = input_size_buffer->get_buffer();
  float *input_program_float_buffer = input_program_buffer->get_buffer();
  float read_color[4];
  float bokeh[4];
  float temp_size[4];
  float multiplier_accum[4];
  float color_accum[4];

  const float max_dim = std::max(get_width(), get_height());
  const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  int max_blur_scalar = tile_data->max_blur_scalar;

  BLI_assert(input_bokeh_buffer->get_width() == COM_BLUR_BOKEH_PIXELS);
  BLI_assert(input_bokeh_buffer->get_height() == COM_BLUR_BOKEH_PIXELS);

#ifdef COM_DEFOCUS_SEARCH
  float search[4];
  input_search_program_->read(search,
                              x / InverseSearchRadiusOperation::DIVIDER,
                              y / InverseSearchRadiusOperation::DIVIDER,
                              nullptr);
  int minx = search[0];
  int miny = search[1];
  int maxx = search[2];
  int maxy = search[3];
#else
  int minx = std::max(x - max_blur_scalar, 0);
  int miny = std::max(y - max_blur_scalar, 0);
  int maxx = std::min(x + max_blur_scalar, int(get_width()));
  int maxy = std::min(y + max_blur_scalar, int(get_height()));
#endif
  {
    input_size_buffer->read_no_check(temp_size, x, y);
    input_program_buffer->read_no_check(read_color, x, y);

    copy_v4_v4(color_accum, read_color);
    copy_v4_fl(multiplier_accum, 1.0f);
    float size_center = temp_size[0] * scalar;

    const int add_xstep_value = QualityStepHelper::get_step();
    const int add_ystep_value = add_xstep_value;
    const int add_xstep_color = add_xstep_value * COM_DATA_TYPE_COLOR_CHANNELS;

    if (size_center > threshold_) {
      for (int ny = miny; ny < maxy; ny += add_ystep_value) {
        float dy = ny - y;
        int offset_value_ny = ny * input_size_buffer->get_width();
        int offset_value_nx_ny = offset_value_ny + (minx);
        int offset_color_nx_ny = offset_value_nx_ny * COM_DATA_TYPE_COLOR_CHANNELS;
        for (int nx = minx; nx < maxx; nx += add_xstep_value) {
          if (nx != x || ny != y) {
            float size = std::min(input_size_float_buffer[offset_value_nx_ny] * scalar,
                                  size_center);
            if (size > threshold_) {
              float dx = nx - x;
              if (size > fabsf(dx) && size > fabsf(dy)) {
                float uv[2] = {
                    float(COM_BLUR_BOKEH_PIXELS / 2) +
                        (dx / size) * float((COM_BLUR_BOKEH_PIXELS / 2) - 1),
                    float(COM_BLUR_BOKEH_PIXELS / 2) +
                        (dy / size) * float((COM_BLUR_BOKEH_PIXELS / 2) - 1),
                };
                input_bokeh_buffer->read(bokeh, uv[0], uv[1]);
                madd_v4_v4v4(color_accum, bokeh, &input_program_float_buffer[offset_color_nx_ny]);
                add_v4_v4(multiplier_accum, bokeh);
              }
            }
          }
          offset_color_nx_ny += add_xstep_color;
          offset_value_nx_ny += add_xstep_value;
        }
      }
    }

    output[0] = color_accum[0] / multiplier_accum[0];
    output[1] = color_accum[1] / multiplier_accum[1];
    output[2] = color_accum[2] / multiplier_accum[2];
    output[3] = color_accum[3] / multiplier_accum[3];

    /* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
    if ((size_center > threshold_) && (size_center < threshold_ * 2.0f)) {
      /* factor from 0-1 */
      float fac = (size_center - threshold_) / threshold_;
      interp_v4_v4v4(output, read_color, output, fac);
    }
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
#ifdef COM_DEFOCUS_SEARCH
  input_search_program_ = nullptr;
#endif
}

bool VariableSizeBokehBlurOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  rcti bokeh_input;

  const float max_dim = std::max(get_width(), get_height());
  const float scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  int max_blur_scalar = max_blur_ * scalar;

  new_input.xmax = input->xmax + max_blur_scalar + 2;
  new_input.xmin = input->xmin - max_blur_scalar + 2;
  new_input.ymax = input->ymax + max_blur_scalar - 2;
  new_input.ymin = input->ymin - max_blur_scalar - 2;
  bokeh_input.xmax = COM_BLUR_BOKEH_PIXELS;
  bokeh_input.xmin = 0;
  bokeh_input.ymax = COM_BLUR_BOKEH_PIXELS;
  bokeh_input.ymin = 0;

  NodeOperation *operation = get_input_operation(2);
  if (operation->determine_depending_area_of_interest(&new_input, read_operation, output)) {
    return true;
  }
  operation = get_input_operation(1);
  if (operation->determine_depending_area_of_interest(&bokeh_input, read_operation, output)) {
    return true;
  }
#ifdef COM_DEFOCUS_SEARCH
  rcti search_input;
  search_input.xmax = (input->xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
  search_input.xmin = (input->xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
  search_input.ymax = (input->ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
  search_input.ymin = (input->ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
  operation = get_input_operation(3);
  if (operation->determine_depending_area_of_interest(&search_input, read_operation, output)) {
    return true;
  }
#endif
  operation = get_input_operation(0);
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

struct PixelData {
  float multiplier_accum[4];
  float color_accum[4];
  float threshold;
  float scalar;
  float size_center;
  int max_blur_scalar;
  int step;
  MemoryBuffer *bokeh_input;
  MemoryBuffer *size_input;
  MemoryBuffer *image_input;
  int image_width;
  int image_height;
};

static void blur_pixel(int x, int y, PixelData &p)
{
  BLI_assert(p.bokeh_input->get_width() == COM_BLUR_BOKEH_PIXELS);
  BLI_assert(p.bokeh_input->get_height() == COM_BLUR_BOKEH_PIXELS);

#ifdef COM_DEFOCUS_SEARCH
  float search[4];
  inputs[DEFOCUS_INPUT_INDEX]->read_elem_checked(x / InverseSearchRadiusOperation::DIVIDER,
                                                 y / InverseSearchRadiusOperation::DIVIDER,
                                                 search);
  const int minx = search[0];
  const int miny = search[1];
  const int maxx = search[2];
  const int maxy = search[3];
#else
  const int minx = std::max(x - p.max_blur_scalar, 0);
  const int miny = std::max(y - p.max_blur_scalar, 0);
  const int maxx = std::min(x + p.max_blur_scalar, p.image_width);
  const int maxy = std::min(y + p.max_blur_scalar, p.image_height);
#endif

  const int color_row_stride = p.image_input->row_stride * p.step;
  const int color_elem_stride = p.image_input->elem_stride * p.step;
  const int size_row_stride = p.size_input->row_stride * p.step;
  const int size_elem_stride = p.size_input->elem_stride * p.step;
  const float *row_color = p.image_input->get_elem(minx, miny);
  const float *row_size = p.size_input->get_elem(minx, miny);
  for (int ny = miny; ny < maxy;
       ny += p.step, row_size += size_row_stride, row_color += color_row_stride)
  {
    const float dy = ny - y;
    const float *size_elem = row_size;
    const float *color = row_color;
    for (int nx = minx; nx < maxx;
         nx += p.step, size_elem += size_elem_stride, color += color_elem_stride)
    {
      if (nx == x && ny == y) {
        continue;
      }
      const float size = std::min(size_elem[0] * p.scalar, p.size_center);
      if (size <= p.threshold) {
        continue;
      }
      const float dx = nx - x;
      if (size <= fabsf(dx) || size <= fabsf(dy)) {
        continue;
      }

      /* XXX: There is no way to ensure bokeh input is an actual bokeh with #COM_BLUR_BOKEH_PIXELS
       * size, anything may be connected. Use the real input size and remove asserts? */
      const float u = float(COM_BLUR_BOKEH_PIXELS / 2) +
                      (dx / size) * float((COM_BLUR_BOKEH_PIXELS / 2) - 1);
      const float v = float(COM_BLUR_BOKEH_PIXELS / 2) +
                      (dy / size) * float((COM_BLUR_BOKEH_PIXELS / 2) - 1);
      float bokeh[4];
      p.bokeh_input->read_elem_checked(u, v, bokeh);
      madd_v4_v4v4(p.color_accum, bokeh, color);
      add_v4_v4(p.multiplier_accum, bokeh);
    }
  }
}

void VariableSizeBokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  PixelData p;
  p.bokeh_input = inputs[BOKEH_INPUT_INDEX];
  p.size_input = inputs[SIZE_INPUT_INDEX];
  p.image_input = inputs[IMAGE_INPUT_INDEX];
  p.step = QualityStepHelper::get_step();
  p.threshold = threshold_;
  p.image_width = this->get_width();
  p.image_height = this->get_height();

  rcti scalar_area = COM_AREA_NONE;
  this->get_area_of_interest(SIZE_INPUT_INDEX, area, scalar_area);
  BLI_rcti_isect(&scalar_area, &p.size_input->get_rect(), &scalar_area);
  const float max_size = p.size_input->get_max_value(scalar_area);

  const float max_dim = std::max(this->get_width(), this->get_height());
  p.scalar = do_size_scale_ ? (max_dim / 100.0f) : 1.0f;
  p.max_blur_scalar = int(max_size * p.scalar);
  CLAMP(p.max_blur_scalar, 1, max_blur_);

  for (BuffersIterator<float> it = output->iterate_with({p.image_input, p.size_input}, area);
       !it.is_end();
       ++it)
  {
    const float *color = it.in(0);
    const float size = *it.in(1);
    copy_v4_v4(p.color_accum, color);
    copy_v4_fl(p.multiplier_accum, 1.0f);
    p.size_center = size * p.scalar;

    if (p.size_center > p.threshold) {
      blur_pixel(it.x, it.y, p);
    }

    it.out[0] = p.color_accum[0] / p.multiplier_accum[0];
    it.out[1] = p.color_accum[1] / p.multiplier_accum[1];
    it.out[2] = p.color_accum[2] / p.multiplier_accum[2];
    it.out[3] = p.color_accum[3] / p.multiplier_accum[3];

    /* Blend in out values over the threshold, otherwise we get sharp, ugly transitions. */
    if ((p.size_center > p.threshold) && (p.size_center < p.threshold * 2.0f)) {
      /* Factor from 0-1. */
      const float fac = (p.size_center - p.threshold) / p.threshold;
      interp_v4_v4v4(it.out, color, it.out, fac);
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
