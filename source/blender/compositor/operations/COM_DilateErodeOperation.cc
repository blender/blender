/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_DilateErodeOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

/* DilateErode Distance Threshold */
DilateErodeThresholdOperation::DilateErodeThresholdOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  this->flags.complex = true;
  this->m_inputProgram = nullptr;
  this->m_inset = 0.0f;
  this->m__switch = 0.5f;
  this->m_distance = 0.0f;
}

void DilateErodeThresholdOperation::init_data()
{
  if (this->m_distance < 0.0f) {
    this->m_scope = -this->m_distance + this->m_inset;
  }
  else {
    if (this->m_inset * 2 > this->m_distance) {
      this->m_scope = MAX2(this->m_inset * 2 - this->m_distance, this->m_distance);
    }
    else {
      this->m_scope = this->m_distance;
    }
  }
  if (this->m_scope < 3) {
    this->m_scope = 3;
  }
}

void DilateErodeThresholdOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
}

void *DilateErodeThresholdOperation::initializeTileData(rcti * /*rect*/)
{
  void *buffer = this->m_inputProgram->initializeTileData(nullptr);
  return buffer;
}

void DilateErodeThresholdOperation::executePixel(float output[4], int x, int y, void *data)
{
  float inputValue[4];
  const float sw = this->m__switch;
  const float distance = this->m_distance;
  float pixelvalue;
  const float rd = this->m_scope * this->m_scope;
  const float inset = this->m_inset;
  float mindist = rd * 2;

  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();
  const rcti &input_rect = inputBuffer->get_rect();
  const int minx = MAX2(x - this->m_scope, input_rect.xmin);
  const int miny = MAX2(y - this->m_scope, input_rect.ymin);
  const int maxx = MIN2(x + this->m_scope, input_rect.xmax);
  const int maxy = MIN2(y + this->m_scope, input_rect.ymax);
  const int bufferWidth = inputBuffer->getWidth();
  int offset;

  inputBuffer->read(inputValue, x, y);
  if (inputValue[0] > sw) {
    for (int yi = miny; yi < maxy; yi++) {
      const float dy = yi - y;
      offset = ((yi - input_rect.ymin) * bufferWidth + (minx - input_rect.xmin));
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
      offset = ((yi - input_rect.ymin) * bufferWidth + (minx - input_rect.xmin));
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

void DilateErodeThresholdOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
}

bool DilateErodeThresholdOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  rcti newInput;

  newInput.xmax = input->xmax + this->m_scope;
  newInput.xmin = input->xmin - this->m_scope;
  newInput.ymax = input->ymax + this->m_scope;
  newInput.ymin = input->ymin - this->m_scope;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DilateErodeThresholdOperation::get_area_of_interest(const int input_idx,
                                                         const rcti &output_area,
                                                         rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - m_scope;
  r_input_area.xmax = output_area.xmax + m_scope;
  r_input_area.ymin = output_area.ymin - m_scope;
  r_input_area.ymax = output_area.ymax + m_scope;
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
  const float *row = p.elem + ((intptr_t)p.ymin - p.y) * p.row_stride +
                     ((intptr_t)p.xmin - p.x) * p.elem_stride;
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
  const float rd = m_scope * m_scope;
  const float inset = m_inset;

  PixelData p;
  p.sw = m__switch;
  p.distance = rd * 2;
  p.elem_stride = input->elem_stride;
  p.row_stride = input->row_stride;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.x = it.x;
    p.y = it.y;
    p.xmin = MAX2(p.x - m_scope, input_rect.xmin);
    p.ymin = MAX2(p.y - m_scope, input_rect.ymin);
    p.xmax = MIN2(p.x + m_scope, input_rect.xmax);
    p.ymax = MIN2(p.y + m_scope, input_rect.ymax);
    p.elem = it.in(0);

    float pixel_value;
    if (*p.elem > p.sw) {
      pixel_value = -sqrtf(get_min_distance<std::less>(p));
    }
    else {
      pixel_value = sqrtf(get_min_distance<std::greater>(p));
    }

    if (m_distance > 0.0f) {
      const float delta = m_distance - pixel_value;
      if (delta >= 0.0f) {
        *it.out = delta >= inset ? 1.0f : delta / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
    else {
      const float delta = -m_distance + pixel_value;
      if (delta < 0.0f) {
        *it.out = delta < -inset ? 1.0f : (-delta) / inset;
      }
      else {
        *it.out = 0.0f;
      }
    }
  }
}

/* Dilate Distance. */
DilateDistanceOperation::DilateDistanceOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  this->m_inputProgram = nullptr;
  this->m_distance = 0.0f;
  flags.complex = true;
  flags.open_cl = true;
}

void DilateDistanceOperation::init_data()
{
  this->m_scope = this->m_distance;
  if (this->m_scope < 3) {
    this->m_scope = 3;
  }
}

void DilateDistanceOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
}

void *DilateDistanceOperation::initializeTileData(rcti * /*rect*/)
{
  void *buffer = this->m_inputProgram->initializeTileData(nullptr);
  return buffer;
}

void DilateDistanceOperation::executePixel(float output[4], int x, int y, void *data)
{
  const float distance = this->m_distance;
  const float mindist = distance * distance;

  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();
  const rcti &input_rect = inputBuffer->get_rect();
  const int minx = MAX2(x - this->m_scope, input_rect.xmin);
  const int miny = MAX2(y - this->m_scope, input_rect.ymin);
  const int maxx = MIN2(x + this->m_scope, input_rect.xmax);
  const int maxy = MIN2(y + this->m_scope, input_rect.ymax);
  const int bufferWidth = inputBuffer->getWidth();
  int offset;

  float value = 0.0f;

  for (int yi = miny; yi < maxy; yi++) {
    const float dy = yi - y;
    offset = ((yi - input_rect.ymin) * bufferWidth + (minx - input_rect.xmin));
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

void DilateDistanceOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
}

bool DilateDistanceOperation::determineDependingAreaOfInterest(rcti *input,
                                                               ReadBufferOperation *readOperation,
                                                               rcti *output)
{
  rcti newInput;

  newInput.xmax = input->xmax + this->m_scope;
  newInput.xmin = input->xmin - this->m_scope;
  newInput.ymax = input->ymax + this->m_scope;
  newInput.ymin = input->ymin - this->m_scope;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DilateDistanceOperation::executeOpenCL(OpenCLDevice *device,
                                            MemoryBuffer *outputMemoryBuffer,
                                            cl_mem clOutputBuffer,
                                            MemoryBuffer **inputMemoryBuffers,
                                            std::list<cl_mem> *clMemToCleanUp,
                                            std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel dilateKernel = device->COM_clCreateKernel("dilateKernel", nullptr);

  cl_int distanceSquared = this->m_distance * this->m_distance;
  cl_int scope = this->m_scope;

  device->COM_clAttachMemoryBufferToKernelParameter(
      dilateKernel, 0, 2, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(dilateKernel, 1, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(dilateKernel, 3, outputMemoryBuffer);
  clSetKernelArg(dilateKernel, 4, sizeof(cl_int), &scope);
  clSetKernelArg(dilateKernel, 5, sizeof(cl_int), &distanceSquared);
  device->COM_clAttachSizeToKernelParameter(dilateKernel, 6, this);
  device->COM_clEnqueueRange(dilateKernel, outputMemoryBuffer, 7, this);
}

void DilateDistanceOperation::get_area_of_interest(const int input_idx,
                                                   const rcti &output_area,
                                                   rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - m_scope;
  r_input_area.xmax = output_area.xmax + m_scope;
  r_input_area.ymin = output_area.ymin - m_scope;
  r_input_area.ymax = output_area.ymax + m_scope;
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
  const float *row = p.elem + ((intptr_t)p.ymin - p.y) * p.row_stride +
                     ((intptr_t)p.xmin - p.x) * p.elem_stride;
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
  PixelData p(inputs[0], m_distance, m_scope);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::greater>(p, 0.0f);
  }
}

/* Erode Distance */
ErodeDistanceOperation::ErodeDistanceOperation() : DilateDistanceOperation()
{
  /* pass */
}

void ErodeDistanceOperation::executePixel(float output[4], int x, int y, void *data)
{
  const float distance = this->m_distance;
  const float mindist = distance * distance;

  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  float *buffer = inputBuffer->getBuffer();
  const rcti &input_rect = inputBuffer->get_rect();
  const int minx = MAX2(x - this->m_scope, input_rect.xmin);
  const int miny = MAX2(y - this->m_scope, input_rect.ymin);
  const int maxx = MIN2(x + this->m_scope, input_rect.xmax);
  const int maxy = MIN2(y + this->m_scope, input_rect.ymax);
  const int bufferWidth = inputBuffer->getWidth();
  int offset;

  float value = 1.0f;

  for (int yi = miny; yi < maxy; yi++) {
    const float dy = yi - y;
    offset = ((yi - input_rect.ymin) * bufferWidth + (minx - input_rect.xmin));
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

void ErodeDistanceOperation::executeOpenCL(OpenCLDevice *device,
                                           MemoryBuffer *outputMemoryBuffer,
                                           cl_mem clOutputBuffer,
                                           MemoryBuffer **inputMemoryBuffers,
                                           std::list<cl_mem> *clMemToCleanUp,
                                           std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel erodeKernel = device->COM_clCreateKernel("erodeKernel", nullptr);

  cl_int distanceSquared = this->m_distance * this->m_distance;
  cl_int scope = this->m_scope;

  device->COM_clAttachMemoryBufferToKernelParameter(
      erodeKernel, 0, 2, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(erodeKernel, 1, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(erodeKernel, 3, outputMemoryBuffer);
  clSetKernelArg(erodeKernel, 4, sizeof(cl_int), &scope);
  clSetKernelArg(erodeKernel, 5, sizeof(cl_int), &distanceSquared);
  device->COM_clAttachSizeToKernelParameter(erodeKernel, 6, this);
  device->COM_clEnqueueRange(erodeKernel, outputMemoryBuffer, 7, this);
}

void ErodeDistanceOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  PixelData p(inputs[0], m_distance, m_scope);
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    p.update(it);
    *it.out = get_distance_value<std::less>(p, 1.0f);
  }
}

/* Dilate step */
DilateStepOperation::DilateStepOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  this->flags.complex = true;
  this->m_inputProgram = nullptr;
}
void DilateStepOperation::initExecution()
{
  this->m_inputProgram = this->getInputSocketReader(0);
}

/* Small helper to pass data from initializeTileData to executePixel. */
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

void *DilateStepOperation::initializeTileData(rcti *rect)
{
  MemoryBuffer *tile = (MemoryBuffer *)this->m_inputProgram->initializeTileData(nullptr);
  int x, y, i;
  int width = tile->getWidth();
  int height = tile->getHeight();
  float *buffer = tile->getBuffer();

  int half_window = this->m_iterations;
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

void DilateStepOperation::executePixel(float output[4], int x, int y, void *data)
{
  tile_info *tile = (tile_info *)data;
  int nx = x - tile->rect.xmin;
  int ny = y - tile->rect.ymin;
  output[0] = tile->buffer[tile->width * ny + nx];
}

void DilateStepOperation::deinitExecution()
{
  this->m_inputProgram = nullptr;
}

void DilateStepOperation::deinitializeTileData(rcti * /*rect*/, void *data)
{
  tile_info *tile = (tile_info *)data;
  MEM_freeN(tile->buffer);
  MEM_freeN(tile);
}

bool DilateStepOperation::determineDependingAreaOfInterest(rcti *input,
                                                           ReadBufferOperation *readOperation,
                                                           rcti *output)
{
  rcti newInput;
  int it = this->m_iterations;
  newInput.xmax = input->xmax + it;
  newInput.xmin = input->xmin - it;
  newInput.ymax = input->ymax + it;
  newInput.ymin = input->ymin - it;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void DilateStepOperation::get_area_of_interest(const int input_idx,
                                               const rcti &output_area,
                                               rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);
  r_input_area.xmin = output_area.xmin - m_iterations;
  r_input_area.xmax = output_area.xmax + m_iterations;
  r_input_area.ymin = output_area.ymin - m_iterations;
  r_input_area.ymax = output_area.ymax + m_iterations;
}

template<typename TCompareSelector>
static void step_update_memory_buffer(MemoryBuffer *output,
                                      const MemoryBuffer *input,
                                      const rcti &area,
                                      const int num_iterations,
                                      const float compare_min_value)
{
  TCompareSelector selector;

  const int width = output->getWidth();
  const int height = output->getHeight();

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
  step_update_memory_buffer<Max2Selector>(output, inputs[0], area, m_iterations, -FLT_MAX);
}

/* Erode step */
ErodeStepOperation::ErodeStepOperation() : DilateStepOperation()
{
  /* pass */
}

void *ErodeStepOperation::initializeTileData(rcti *rect)
{
  MemoryBuffer *tile = (MemoryBuffer *)this->m_inputProgram->initializeTileData(nullptr);
  int x, y, i;
  int width = tile->getWidth();
  int height = tile->getHeight();
  float *buffer = tile->getBuffer();

  int half_window = this->m_iterations;
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
  step_update_memory_buffer<Min2Selector>(output, inputs[0], area, m_iterations, FLT_MAX);
}

}  // namespace blender::compositor
