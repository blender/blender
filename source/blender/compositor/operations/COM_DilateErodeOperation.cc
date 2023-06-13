/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DilateErodeOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

DilateErodeThresholdOperation::DilateErodeThresholdOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.complex = true;
  input_program_ = nullptr;
  inset_ = 0.0f;
  switch_ = 0.5f;
  distance_ = 0.0f;
}

void DilateErodeThresholdOperation::init_data()
{
  if (distance_ < 0.0f) {
    scope_ = -distance_ + inset_;
  }
  else {
    if (inset_ * 2 > distance_) {
      scope_ = MAX2(inset_ * 2 - distance_, distance_);
    }
    else {
      scope_ = distance_;
    }
  }
  if (scope_ < 3) {
    scope_ = 3;
  }
}

void DilateErodeThresholdOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void *DilateErodeThresholdOperation::initialize_tile_data(rcti * /*rect*/)
{
  void *buffer = input_program_->initialize_tile_data(nullptr);
  return buffer;
}

void DilateErodeThresholdOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float input_value[4];
  const float sw = switch_;
  const float distance = distance_;
  float pixelvalue;
  const float rd = scope_ * scope_;
  const float inset = inset_;
  float mindist = rd * 2;

  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();
  const rcti &input_rect = input_buffer->get_rect();
  const int minx = MAX2(x - scope_, input_rect.xmin);
  const int miny = MAX2(y - scope_, input_rect.ymin);
  const int maxx = MIN2(x + scope_, input_rect.xmax);
  const int maxy = MIN2(y + scope_, input_rect.ymax);
  const int buffer_width = input_buffer->get_width();
  int offset;

  input_buffer->read(input_value, x, y);
  if (input_value[0] > sw) {
    for (int yi = miny; yi < maxy; yi++) {
      const float dy = yi - y;
      offset = ((yi - input_rect.ymin) * buffer_width + (minx - input_rect.xmin));
      for (int xi = minx; xi < maxx; xi++) {
        if (buffer[offset] < sw) {
          const float dx = xi - x;
          const float dis = dx * dx + dy * dy;
          mindist = MIN2(mindist, dis);
        }
        offset++;
      }
    }
    pixelvalue = -sqrtf(mindist);
  }
  else {
    for (int yi = miny; yi < maxy; yi++) {
      const float dy = yi - y;
      offset = ((yi - input_rect.ymin) * buffer_width + (minx - input_rect.xmin));
      for (int xi = minx; xi < maxx; xi++) {
        if (buffer[offset] > sw) {
          const float dx = xi - x;
          const float dis = dx * dx + dy * dy;
          mindist = MIN2(mindist, dis);
        }
        offset++;
      }
    }
    pixelvalue = sqrtf(mindist);
  }

  if (distance > 0.0f) {
    const float delta = distance - pixelvalue;
    if (delta >= 0.0f) {
      if (delta >= inset) {
        output[0] = 1.0f;
      }
      else {
        output[0] = delta / inset;
      }
    }
    else {
      output[0] = 0.0f;
    }
  }
  else {
    const float delta = -distance + pixelvalue;
    if (delta < 0.0f) {
      if (delta < -inset) {
        output[0] = 1.0f;
      }
      else {
        output[0] = (-delta) / inset;
      }
    }
    else {
      output[0] = 0.0f;
    }
  }
}

void DilateErodeThresholdOperation::deinit_execution()
{
  input_program_ = nullptr;
}

bool DilateErodeThresholdOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = input->xmax + scope_;
  new_input.xmin = input->xmin - scope_;
  new_input.ymax = input->ymax + scope_;
  new_input.ymin = input->ymin - scope_;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void DilateErodeThresholdOperation::get_area_of_interest(const int input_idx,
                                                         const rcti &output_area,
                                                         rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - scope_;
  r_input_area.xmax = output_area.xmax + scope_;
  r_input_area.ymin = output_area.ymin - scope_;
  r_input_area.ymax = output_area.ymax + scope_;
}

struct DilateErodeThresholdOperation::PixelData {
  int x;
  int y;
  int xmin;
  int xmax;
  int ymin;
  int ymax;
  const float *elem;
  float distance;
  int elem_stride;
  int row_stride;
  /** Switch. */
  float sw;
};

template<template<typename> typename TCompare>
static float get_min_distance(DilateErodeThresholdOperation::PixelData &p)
{
  /* TODO(manzanilla): bad performance, generate a table with relative offsets on operation
   * initialization to loop from less to greater distance and break as soon as #compare is
   * true. */
  const TCompare compare;
  float min_dist = p.distance;
  const float *row = p.elem + (intptr_t(p.ymin) - p.y) * p.row_stride +
                     (intptr_t(p.xmin) - p.x) * p.elem_stride;
  for (int yi = p.ymin; yi < p.ymax; yi++) {
    const float dy = yi - p.y;
    const float dist_y = dy * dy;
    const float *elem = row;
    for (int xi = p.xmin; xi < p.xmax; xi++) {
      if (compare(*elem, p.sw)) {
        const float dx = xi - p.x;
        const float dist = dx * dx + dist_y;
        min_dist = MIN2(min_dist, dist);
      }
      elem += p.elem_stride;
    }
    row += p.row_stride;
  }
  return min_dist;
}

void DilateErodeThresholdOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const rcti &input_rect = input->get_rect();
  const float rd = scope_ * scope_;
  const float inset = inset_;

  PixelData p;
  p.sw = switch_;
  p.distance = rd * 2;
  p.elem_stride = input->elem_stride;
  p.row_stride = input->row_stride;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.x = it.x;
    p.y = it.y;
    p.xmin = MAX2(p.x - scope_, input_rect.xmin);
    p.ymin = MAX2(p.y - scope_, input_rect.ymin);
    p.xmax = MIN2(p.x + scope_, input_rect.xmax);
    p.ymax = MIN2(p.y + scope_, input_rect.ymax);
    p.elem = it.in(0);

    float pixel_value;
    if (*p.elem > p.sw) {
      pixel_value = -sqrtf(get_min_distance<std::less>(p));
    }
    else {
      pixel_value = sqrtf(get_min_distance<std::greater>(p));
    }

    if (distance_ > 0.0f) {
      const float delta = distance_ - pixel_value;
      if (delta >= 0.0f) {
        *it.out = delta >= inset ? 1.0f : delta / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
    else {
      const float delta = -distance_ + pixel_value;
      if (delta < 0.0f) {
        *it.out = delta < -inset ? 1.0f : (-delta) / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
  }
}

DilateDistanceOperation::DilateDistanceOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_program_ = nullptr;
  distance_ = 0.0f;
  flags_.complex = true;
  flags_.open_cl = true;
}

void DilateDistanceOperation::init_data()
{
  scope_ = distance_;
  if (scope_ < 3) {
    scope_ = 3;
  }
}

void DilateDistanceOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void *DilateDistanceOperation::initialize_tile_data(rcti * /*rect*/)
{
  void *buffer = input_program_->initialize_tile_data(nullptr);
  return buffer;
}

void DilateDistanceOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const float distance = distance_;
  const float mindist = distance * distance;

  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();
  const rcti &input_rect = input_buffer->get_rect();
  const int minx = MAX2(x - scope_, input_rect.xmin);
  const int miny = MAX2(y - scope_, input_rect.ymin);
  const int maxx = MIN2(x + scope_, input_rect.xmax);
  const int maxy = MIN2(y + scope_, input_rect.ymax);
  const int buffer_width = input_buffer->get_width();
  int offset;

  float value = 0.0f;

  for (int yi = miny; yi < maxy; yi++) {
    const float dy = yi - y;
    offset = ((yi - input_rect.ymin) * buffer_width + (minx - input_rect.xmin));
    for (int xi = minx; xi < maxx; xi++) {
      const float dx = xi - x;
      const float dis = dx * dx + dy * dy;
      if (dis <= mindist) {
        value = MAX2(buffer[offset], value);
      }
      offset++;
    }
  }
  output[0] = value;
}

void DilateDistanceOperation::deinit_execution()
{
  input_program_ = nullptr;
}

bool DilateDistanceOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = input->xmax + scope_;
  new_input.xmin = input->xmin - scope_;
  new_input.ymax = input->ymax + scope_;
  new_input.ymin = input->ymin - scope_;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void DilateDistanceOperation::execute_opencl(OpenCLDevice *device,
                                             MemoryBuffer *output_memory_buffer,
                                             cl_mem cl_output_buffer,
                                             MemoryBuffer **input_memory_buffers,
                                             std::list<cl_mem> *cl_mem_to_clean_up,
                                             std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel dilate_kernel = device->COM_cl_create_kernel("dilate_kernel", nullptr);

  cl_int distance_squared = distance_ * distance_;
  cl_int scope = scope_;

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      dilate_kernel, 0, 2, cl_mem_to_clean_up, input_memory_buffers, input_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(
      dilate_kernel, 1, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
      dilate_kernel, 3, output_memory_buffer);
  clSetKernelArg(dilate_kernel, 4, sizeof(cl_int), &scope);
  clSetKernelArg(dilate_kernel, 5, sizeof(cl_int), &distance_squared);
  device->COM_cl_attach_size_to_kernel_parameter(dilate_kernel, 6, this);
  device->COM_cl_enqueue_range(dilate_kernel, output_memory_buffer, 7, this);
}

void DilateDistanceOperation::get_area_of_interest(const int input_idx,
                                                   const rcti &output_area,
                                                   rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - scope_;
  r_input_area.xmax = output_area.xmax + scope_;
  r_input_area.ymin = output_area.ymin - scope_;
  r_input_area.ymax = output_area.ymax + scope_;
}

struct DilateDistanceOperation::PixelData {
  int x;
  int y;
  int xmin;
  int xmax;
  int ymin;
  int ymax;
  const float *elem;
  float min_distance;
  int scope;
  int elem_stride;
  int row_stride;
  const rcti &input_rect;

  PixelData(MemoryBuffer *input, const int distance, const int scope)
      : min_distance(distance * distance),
        scope(scope),
        elem_stride(input->elem_stride),
        row_stride(input->row_stride),
        input_rect(input->get_rect())
  {
  }

  void update(BuffersIterator<float> &it)
  {
    x = it.x;
    y = it.y;
    xmin = MAX2(x - scope, input_rect.xmin);
    ymin = MAX2(y - scope, input_rect.ymin);
    xmax = MIN2(x + scope, input_rect.xmax);
    ymax = MIN2(y + scope, input_rect.ymax);
    elem = it.in(0);
  }
};

template<template<typename> typename TCompare>
static float get_distance_value(DilateDistanceOperation::PixelData &p, const float start_value)
{
  /* TODO(manzanilla): bad performance, only loop elements within minimum distance removing
   * coordinates and conditional if `dist <= min_dist`. May need to generate a table of offsets. */
  const TCompare compare;
  const float min_dist = p.min_distance;
  float value = start_value;
  const float *row = p.elem + (intptr_t(p.ymin) - p.y) * p.row_stride +
                     (intptr_t(p.xmin) - p.x) * p.elem_stride;
  for (int yi = p.ymin; yi < p.ymax; yi++) {
    const float dy = yi - p.y;
    const float dist_y = dy * dy;
    const float *elem = row;
    for (int xi = p.xmin; xi < p.xmax; xi++) {
      const float dx = xi - p.x;
      const float dist = dx * dx + dist_y;
      if (dist <= min_dist) {
        value = compare(*elem, value) ? *elem : value;
      }
      elem += p.elem_stride;
    }
    row += p.row_stride;
  }

  return value;
}

void DilateDistanceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  PixelData p(inputs[0], distance_, scope_);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::greater>(p, 0.0f);
  }
}

ErodeDistanceOperation::ErodeDistanceOperation() : DilateDistanceOperation()
{
  /* pass */
}

void ErodeDistanceOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  const float distance = distance_;
  const float mindist = distance * distance;

  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  float *buffer = input_buffer->get_buffer();
  const rcti &input_rect = input_buffer->get_rect();
  const int minx = MAX2(x - scope_, input_rect.xmin);
  const int miny = MAX2(y - scope_, input_rect.ymin);
  const int maxx = MIN2(x + scope_, input_rect.xmax);
  const int maxy = MIN2(y + scope_, input_rect.ymax);
  const int buffer_width = input_buffer->get_width();
  int offset;

  float value = 1.0f;

  for (int yi = miny; yi < maxy; yi++) {
    const float dy = yi - y;
    offset = ((yi - input_rect.ymin) * buffer_width + (minx - input_rect.xmin));
    for (int xi = minx; xi < maxx; xi++) {
      const float dx = xi - x;
      const float dis = dx * dx + dy * dy;
      if (dis <= mindist) {
        value = MIN2(buffer[offset], value);
      }
      offset++;
    }
  }
  output[0] = value;
}

void ErodeDistanceOperation::execute_opencl(OpenCLDevice *device,
                                            MemoryBuffer *output_memory_buffer,
                                            cl_mem cl_output_buffer,
                                            MemoryBuffer **input_memory_buffers,
                                            std::list<cl_mem> *cl_mem_to_clean_up,
                                            std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
{
  cl_kernel erode_kernel = device->COM_cl_create_kernel("erode_kernel", nullptr);

  cl_int distance_squared = distance_ * distance_;
  cl_int scope = scope_;

  device->COM_cl_attach_memory_buffer_to_kernel_parameter(
      erode_kernel, 0, 2, cl_mem_to_clean_up, input_memory_buffers, input_program_);
  device->COM_cl_attach_output_memory_buffer_to_kernel_parameter(
      erode_kernel, 1, cl_output_buffer);
  device->COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
      erode_kernel, 3, output_memory_buffer);
  clSetKernelArg(erode_kernel, 4, sizeof(cl_int), &scope);
  clSetKernelArg(erode_kernel, 5, sizeof(cl_int), &distance_squared);
  device->COM_cl_attach_size_to_kernel_parameter(erode_kernel, 6, this);
  device->COM_cl_enqueue_range(erode_kernel, output_memory_buffer, 7, this);
}

void ErodeDistanceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  PixelData p(inputs[0], distance_, scope_);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::less>(p, 1.0f);
  }
}

DilateStepOperation::DilateStepOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.complex = true;
  input_program_ = nullptr;
}
void DilateStepOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

/* Small helper to pass data from initialize_tile_data to execute_pixel. */
struct tile_info {
  rcti rect;
  int width;
  float *buffer;
};

static tile_info *create_cache(int xmin, int xmax, int ymin, int ymax)
{
  tile_info *result = (tile_info *)MEM_mallocN(sizeof(tile_info), "dilate erode tile");
  result->rect.xmin = xmin;
  result->rect.xmax = xmax;
  result->rect.ymin = ymin;
  result->rect.ymax = ymax;
  result->width = xmax - xmin;
  result->buffer = (float *)MEM_callocN(sizeof(float) * (ymax - ymin) * result->width,
                                        "dilate erode cache");
  return result;
}

void *DilateStepOperation::initialize_tile_data(rcti *rect)
{
  MemoryBuffer *tile = (MemoryBuffer *)input_program_->initialize_tile_data(nullptr);
  int x, y, i;
  int width = tile->get_width();
  int height = tile->get_height();
  float *buffer = tile->get_buffer();

  int half_window = iterations_;
  int window = half_window * 2 + 1;

  int xmin = MAX2(0, rect->xmin - half_window);
  int ymin = MAX2(0, rect->ymin - half_window);
  int xmax = MIN2(width, rect->xmax + half_window);
  int ymax = MIN2(height, rect->ymax + half_window);

  int bwidth = rect->xmax - rect->xmin;
  int bheight = rect->ymax - rect->ymin;

  /* NOTE: Cache buffer has original tile-size width, but new height.
   * We have to calculate the additional rows in the first pass,
   * to have valid data available for the second pass. */
  tile_info *result = create_cache(rect->xmin, rect->xmax, ymin, ymax);
  float *rectf = result->buffer;

  /* temp holds maxima for every step in the algorithm, buf holds a
   * single row or column of input values, padded with FLT_MAX's to
   * simplify the logic. */
  float *temp = (float *)MEM_mallocN(sizeof(float) * (2 * window - 1), "dilate erode temp");
  float *buf = (float *)MEM_mallocN(sizeof(float) * (MAX2(bwidth, bheight) + 5 * half_window),
                                    "dilate erode buf");

  /* The following is based on the van Herk/Gil-Werman algorithm for morphology operations.
   * first pass, horizontal dilate/erode. */
  for (y = ymin; y < ymax; y++) {
    for (x = 0; x < bwidth + 5 * half_window; x++) {
      buf[x] = -FLT_MAX;
    }
    for (x = xmin; x < xmax; x++) {
      buf[x - rect->xmin + window - 1] = buffer[(y * width + x)];
    }

    for (i = 0; i < (bwidth + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (x = 1; x < window; x++) {
        temp[window - 1 - x] = MAX2(temp[window - x], buf[start - x]);
        temp[window - 1 + x] = MAX2(temp[window + x - 2], buf[start + x]);
      }

      start = half_window + (i - 1) * window + 1;
      for (x = -MIN2(0, start); x < window - MAX2(0, start + window - bwidth); x++) {
        rectf[bwidth * (y - ymin) + (start + x)] = MAX2(temp[x], temp[x + window - 1]);
      }
    }
  }

  /* Second pass, vertical dilate/erode. */
  for (x = 0; x < bwidth; x++) {
    for (y = 0; y < bheight + 5 * half_window; y++) {
      buf[y] = -FLT_MAX;
    }
    for (y = ymin; y < ymax; y++) {
      buf[y - rect->ymin + window - 1] = rectf[(y - ymin) * bwidth + x];
    }

    for (i = 0; i < (bheight + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (y = 1; y < window; y++) {
        temp[window - 1 - y] = MAX2(temp[window - y], buf[start - y]);
        temp[window - 1 + y] = MAX2(temp[window + y - 2], buf[start + y]);
      }

      start = half_window + (i - 1) * window + 1;
      for (y = -MIN2(0, start); y < window - MAX2(0, start + window - bheight); y++) {
        rectf[bwidth * (y + start + (rect->ymin - ymin)) + x] = MAX2(temp[y],
                                                                     temp[y + window - 1]);
      }
    }
  }

  MEM_freeN(temp);
  MEM_freeN(buf);

  return result;
}

void DilateStepOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  tile_info *tile = (tile_info *)data;
  int nx = x - tile->rect.xmin;
  int ny = y - tile->rect.ymin;
  output[0] = tile->buffer[tile->width * ny + nx];
}

void DilateStepOperation::deinit_execution()
{
  input_program_ = nullptr;
}

void DilateStepOperation::deinitialize_tile_data(rcti * /*rect*/, void *data)
{
  tile_info *tile = (tile_info *)data;
  MEM_freeN(tile->buffer);
  MEM_freeN(tile);
}

bool DilateStepOperation::determine_depending_area_of_interest(rcti *input,
                                                               ReadBufferOperation *read_operation,
                                                               rcti *output)
{
  rcti new_input;
  int it = iterations_;
  new_input.xmax = input->xmax + it;
  new_input.xmin = input->xmin - it;
  new_input.ymax = input->ymax + it;
  new_input.ymin = input->ymin - it;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void DilateStepOperation::get_area_of_interest(const int input_idx,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - iterations_;
  r_input_area.xmax = output_area.xmax + iterations_;
  r_input_area.ymin = output_area.ymin - iterations_;
  r_input_area.ymax = output_area.ymax + iterations_;
}

template<typename TCompareSelector>
static void step_update_memory_buffer(MemoryBuffer *output,
                                      const MemoryBuffer *input,
                                      const rcti &area,
                                      const int num_iterations,
                                      const float compare_min_value)
{
  TCompareSelector selector;

  const int width = output->get_width();
  const int height = output->get_height();

  const int half_window = num_iterations;
  const int window = half_window * 2 + 1;

  const int xmin = MAX2(0, area.xmin - half_window);
  const int ymin = MAX2(0, area.ymin - half_window);
  const int xmax = MIN2(width, area.xmax + half_window);
  const int ymax = MIN2(height, area.ymax + half_window);

  const int bwidth = area.xmax - area.xmin;
  const int bheight = area.ymax - area.ymin;

  /* NOTE: #result has area width, but new height.
   * We have to calculate the additional rows in the first pass,
   * to have valid data available for the second pass. */
  rcti result_area;
  BLI_rcti_init(&result_area, area.xmin, area.xmax, ymin, ymax);
  MemoryBuffer result(DataType::Value, result_area);

  /* #temp holds maxima for every step in the algorithm, #buf holds a
   * single row or column of input values, padded with #limit values to
   * simplify the logic. */
  float *temp = (float *)MEM_mallocN(sizeof(float) * (2 * window - 1), "dilate erode temp");
  float *buf = (float *)MEM_mallocN(sizeof(float) * (MAX2(bwidth, bheight) + 5 * half_window),
                                    "dilate erode buf");

  /* The following is based on the van Herk/Gil-Werman algorithm for morphology operations. */
  /* First pass, horizontal dilate/erode. */
  for (int y = ymin; y < ymax; y++) {
    for (int x = 0; x < bwidth + 5 * half_window; x++) {
      buf[x] = compare_min_value;
    }
    for (int x = xmin; x < xmax; x++) {
      buf[x - area.xmin + window - 1] = input->get_value(x, y, 0);
    }

    for (int i = 0; i < (bwidth + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (int x = 1; x < window; x++) {
        temp[window - 1 - x] = selector(temp[window - x], buf[start - x]);
        temp[window - 1 + x] = selector(temp[window + x - 2], buf[start + x]);
      }

      start = half_window + (i - 1) * window + 1;
      for (int x = -MIN2(0, start); x < window - MAX2(0, start + window - bwidth); x++) {
        result.get_value(start + x + area.xmin, y, 0) = selector(temp[x], temp[x + window - 1]);
      }
    }
  }

  /* Second pass, vertical dilate/erode. */
  for (int x = 0; x < bwidth; x++) {
    for (int y = 0; y < bheight + 5 * half_window; y++) {
      buf[y] = compare_min_value;
    }
    for (int y = ymin; y < ymax; y++) {
      buf[y - area.ymin + window - 1] = result.get_value(x + area.xmin, y, 0);
    }

    for (int i = 0; i < (bheight + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (int y = 1; y < window; y++) {
        temp[window - 1 - y] = selector(temp[window - y], buf[start - y]);
        temp[window - 1 + y] = selector(temp[window + y - 2], buf[start + y]);
      }

      start = half_window + (i - 1) * window + 1;
      for (int y = -MIN2(0, start); y < window - MAX2(0, start + window - bheight); y++) {
        result.get_value(x + area.xmin, y + start + area.ymin, 0) = selector(temp[y],
                                                                             temp[y + window - 1]);
      }
    }
  }

  MEM_freeN(temp);
  MEM_freeN(buf);

  output->copy_from(&result, area);
}

struct Max2Selector {
  float operator()(float f1, float f2) const
  {
    return MAX2(f1, f2);
  }
};

void DilateStepOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  step_update_memory_buffer<Max2Selector>(output, inputs[0], area, iterations_, -FLT_MAX);
}

ErodeStepOperation::ErodeStepOperation() : DilateStepOperation()
{
  /* pass */
}

void *ErodeStepOperation::initialize_tile_data(rcti *rect)
{
  MemoryBuffer *tile = (MemoryBuffer *)input_program_->initialize_tile_data(nullptr);
  int x, y, i;
  int width = tile->get_width();
  int height = tile->get_height();
  float *buffer = tile->get_buffer();

  int half_window = iterations_;
  int window = half_window * 2 + 1;

  int xmin = MAX2(0, rect->xmin - half_window);
  int ymin = MAX2(0, rect->ymin - half_window);
  int xmax = MIN2(width, rect->xmax + half_window);
  int ymax = MIN2(height, rect->ymax + half_window);

  int bwidth = rect->xmax - rect->xmin;
  int bheight = rect->ymax - rect->ymin;

  /* NOTE: Cache buffer has original tile-size width, but new height.
   * We have to calculate the additional rows in the first pass,
   * to have valid data available for the second pass. */
  tile_info *result = create_cache(rect->xmin, rect->xmax, ymin, ymax);
  float *rectf = result->buffer;

  /* temp holds maxima for every step in the algorithm, buf holds a
   * single row or column of input values, padded with FLT_MAX's to
   * simplify the logic. */
  float *temp = (float *)MEM_mallocN(sizeof(float) * (2 * window - 1), "dilate erode temp");
  float *buf = (float *)MEM_mallocN(sizeof(float) * (MAX2(bwidth, bheight) + 5 * half_window),
                                    "dilate erode buf");

  /* The following is based on the van Herk/Gil-Werman algorithm for morphology operations.
   * first pass, horizontal dilate/erode */
  for (y = ymin; y < ymax; y++) {
    for (x = 0; x < bwidth + 5 * half_window; x++) {
      buf[x] = FLT_MAX;
    }
    for (x = xmin; x < xmax; x++) {
      buf[x - rect->xmin + window - 1] = buffer[(y * width + x)];
    }

    for (i = 0; i < (bwidth + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (x = 1; x < window; x++) {
        temp[window - 1 - x] = MIN2(temp[window - x], buf[start - x]);
        temp[window - 1 + x] = MIN2(temp[window + x - 2], buf[start + x]);
      }

      start = half_window + (i - 1) * window + 1;
      for (x = -MIN2(0, start); x < window - MAX2(0, start + window - bwidth); x++) {
        rectf[bwidth * (y - ymin) + (start + x)] = MIN2(temp[x], temp[x + window - 1]);
      }
    }
  }

  /* Second pass, vertical dilate/erode. */
  for (x = 0; x < bwidth; x++) {
    for (y = 0; y < bheight + 5 * half_window; y++) {
      buf[y] = FLT_MAX;
    }
    for (y = ymin; y < ymax; y++) {
      buf[y - rect->ymin + window - 1] = rectf[(y - ymin) * bwidth + x];
    }

    for (i = 0; i < (bheight + 3 * half_window) / window; i++) {
      int start = (i + 1) * window - 1;

      temp[window - 1] = buf[start];
      for (y = 1; y < window; y++) {
        temp[window - 1 - y] = MIN2(temp[window - y], buf[start - y]);
        temp[window - 1 + y] = MIN2(temp[window + y - 2], buf[start + y]);
      }

      start = half_window + (i - 1) * window + 1;
      for (y = -MIN2(0, start); y < window - MAX2(0, start + window - bheight); y++) {
        rectf[bwidth * (y + start + (rect->ymin - ymin)) + x] = MIN2(temp[y],
                                                                     temp[y + window - 1]);
      }
    }
  }

  MEM_freeN(temp);
  MEM_freeN(buf);

  return result;
}

struct Min2Selector {
  float operator()(float f1, float f2) const
  {
    return MIN2(f1, f2);
  }
};

void ErodeStepOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  step_update_memory_buffer<Min2Selector>(output, inputs[0], area, iterations_, FLT_MAX);
}

}  // namespace blender::compositor
