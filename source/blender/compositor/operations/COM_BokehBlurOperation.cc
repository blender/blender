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
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Color, ResizeMode::Align);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Color);

  flags.complex = true;
  flags.open_cl = true;

  size_ = 1.0f;
  sizeavailable_ = false;
  inputProgram_ = nullptr;
  inputBokehProgram_ = nullptr;
  inputBoundingBoxReader_ = nullptr;

  extend_bounds_ = false;
}

void BokehBlurOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    updateSize();
  }

  NodeOperation *bokeh = get_input_operation(BOKEH_INPUT_INDEX);
  const int width = bokeh->getWidth();
  const int height = bokeh->getHeight();

  const float dimension = MIN2(width, height);

  bokehMidX_ = width / 2.0f;
  bokehMidY_ = height / 2.0f;
  bokehDimension_ = dimension / 2.0f;
}

void *BokehBlurOperation::initializeTileData(rcti * /*rect*/)
{
  lockMutex();
  if (!sizeavailable_) {
    updateSize();
  }
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  unlockMutex();
  return buffer;
}

void BokehBlurOperation::initExecution()
{
  initMutex();

  inputProgram_ = getInputSocketReader(0);
  inputBokehProgram_ = getInputSocketReader(1);
  inputBoundingBoxReader_ = getInputSocketReader(2);

  QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void BokehBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  float color_accum[4];
  float tempBoundingBox[4];
  float bokeh[4];

  inputBoundingBoxReader_->readSampled(tempBoundingBox, x, y, PixelSampler::Nearest);
  if (tempBoundingBox[0] > 0.0f) {
    float multiplier_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
    const rcti &input_rect = inputBuffer->get_rect();
    float *buffer = inputBuffer->getBuffer();
    int bufferwidth = inputBuffer->getWidth();
    int bufferstartx = input_rect.xmin;
    int bufferstarty = input_rect.ymin;
    const float max_dim = MAX2(this->getWidth(), this->getHeight());
    int pixelSize = size_ * max_dim / 100.0f;
    zero_v4(color_accum);

    if (pixelSize < 2) {
      inputProgram_->readSampled(color_accum, x, y, PixelSampler::Nearest);
      multiplier_accum[0] = 1.0f;
      multiplier_accum[1] = 1.0f;
      multiplier_accum[2] = 1.0f;
      multiplier_accum[3] = 1.0f;
    }
    int miny = y - pixelSize;
    int maxy = y + pixelSize;
    int minx = x - pixelSize;
    int maxx = x + pixelSize;
    miny = MAX2(miny, input_rect.ymin);
    minx = MAX2(minx, input_rect.xmin);
    maxy = MIN2(maxy, input_rect.ymax);
    maxx = MIN2(maxx, input_rect.xmax);

    int step = getStep();
    int offsetadd = getOffsetAdd() * COM_DATA_TYPE_COLOR_CHANNELS;

    float m = bokehDimension_ / pixelSize;
    for (int ny = miny; ny < maxy; ny += step) {
      int bufferindex = ((minx - bufferstartx) * COM_DATA_TYPE_COLOR_CHANNELS) +
                        ((ny - bufferstarty) * COM_DATA_TYPE_COLOR_CHANNELS * bufferwidth);
      for (int nx = minx; nx < maxx; nx += step) {
        float u = bokehMidX_ - (nx - x) * m;
        float v = bokehMidY_ - (ny - y) * m;
        inputBokehProgram_->readSampled(bokeh, u, v, PixelSampler::Nearest);
        madd_v4_v4v4(color_accum, bokeh, &buffer[bufferindex]);
        add_v4_v4(multiplier_accum, bokeh);
        bufferindex += offsetadd;
      }
    }
    output[0] = color_accum[0] * (1.0f / multiplier_accum[0]);
    output[1] = color_accum[1] * (1.0f / multiplier_accum[1]);
    output[2] = color_accum[2] * (1.0f / multiplier_accum[2]);
    output[3] = color_accum[3] * (1.0f / multiplier_accum[3]);
  }
  else {
    inputProgram_->readSampled(output, x, y, PixelSampler::Nearest);
  }
}

void BokehBlurOperation::deinitExecution()
{
  deinitMutex();
  inputProgram_ = nullptr;
  inputBokehProgram_ = nullptr;
  inputBoundingBoxReader_ = nullptr;
}

bool BokehBlurOperation::determineDependingAreaOfInterest(rcti *input,
                                                          ReadBufferOperation *readOperation,
                                                          rcti *output)
{
  rcti newInput;
  rcti bokehInput;
  const float max_dim = MAX2(this->getWidth(), this->getHeight());

  if (sizeavailable_) {
    newInput.xmax = input->xmax + (size_ * max_dim / 100.0f);
    newInput.xmin = input->xmin - (size_ * max_dim / 100.0f);
    newInput.ymax = input->ymax + (size_ * max_dim / 100.0f);
    newInput.ymin = input->ymin - (size_ * max_dim / 100.0f);
  }
  else {
    newInput.xmax = input->xmax + (10.0f * max_dim / 100.0f);
    newInput.xmin = input->xmin - (10.0f * max_dim / 100.0f);
    newInput.ymax = input->ymax + (10.0f * max_dim / 100.0f);
    newInput.ymin = input->ymin - (10.0f * max_dim / 100.0f);
  }

  NodeOperation *operation = getInputOperation(1);
  bokehInput.xmax = operation->getWidth();
  bokehInput.xmin = 0;
  bokehInput.ymax = operation->getHeight();
  bokehInput.ymin = 0;
  if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output)) {
    return true;
  }
  operation = getInputOperation(0);
  if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output)) {
    return true;
  }
  operation = getInputOperation(2);
  if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
    return true;
  }
  if (!sizeavailable_) {
    rcti sizeInput;
    sizeInput.xmin = 0;
    sizeInput.ymin = 0;
    sizeInput.xmax = 5;
    sizeInput.ymax = 5;
    operation = getInputOperation(3);
    if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
      return true;
    }
  }
  return false;
}

void BokehBlurOperation::executeOpenCL(OpenCLDevice *device,
                                       MemoryBuffer *outputMemoryBuffer,
                                       cl_mem clOutputBuffer,
                                       MemoryBuffer **inputMemoryBuffers,
                                       std::list<cl_mem> *clMemToCleanUp,
                                       std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel kernel = device->COM_clCreateKernel("bokehBlurKernel", nullptr);
  if (!sizeavailable_) {
    updateSize();
  }
  const float max_dim = MAX2(this->getWidth(), this->getHeight());
  cl_int radius = size_ * max_dim / 100.0f;
  cl_int step = this->getStep();

  device->COM_clAttachMemoryBufferToKernelParameter(
      kernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, inputBoundingBoxReader_);
  device->COM_clAttachMemoryBufferToKernelParameter(
      kernel, 1, 4, clMemToCleanUp, inputMemoryBuffers, inputProgram_);
  device->COM_clAttachMemoryBufferToKernelParameter(
      kernel, 2, -1, clMemToCleanUp, inputMemoryBuffers, inputBokehProgram_);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(kernel, 3, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(kernel, 5, outputMemoryBuffer);
  clSetKernelArg(kernel, 6, sizeof(cl_int), &radius);
  clSetKernelArg(kernel, 7, sizeof(cl_int), &step);
  device->COM_clAttachSizeToKernelParameter(kernel, 8, this);

  device->COM_clEnqueueRange(kernel, outputMemoryBuffer, 9, this);
}

void BokehBlurOperation::updateSize()
{
  if (sizeavailable_) {
    return;
  }

  switch (execution_model_) {
    case eExecutionModel::Tiled: {
      float result[4];
      this->getInputSocketReader(3)->readSampled(result, 0, 0, PixelSampler::Nearest);
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
      const float max_dim = MAX2(BLI_rcti_size_x(&r_area), BLI_rcti_size_y(&r_area));
      r_area.xmax += 2 * size_ * max_dim / 100.0f;
      r_area.ymax += 2 * size_ * max_dim / 100.0f;
      break;
    }
    case eExecutionModel::FullFrame: {
      set_determined_canvas_modifier([=](rcti &canvas) {
        const float max_dim = MAX2(BLI_rcti_size_x(&canvas), BLI_rcti_size_y(&canvas));
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
      const float max_dim = MAX2(this->getWidth(), this->getHeight());
      const float add_size = size_ * max_dim / 100.0f;
      r_input_area.xmin = output_area.xmin - add_size;
      r_input_area.xmax = output_area.xmax + add_size;
      r_input_area.ymin = output_area.ymin - add_size;
      r_input_area.ymax = output_area.ymax + add_size;
      break;
    }
    case BOKEH_INPUT_INDEX: {
      NodeOperation *bokeh_input = getInputOperation(BOKEH_INPUT_INDEX);
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
  const float max_dim = MAX2(this->getWidth(), this->getHeight());
  const int pixel_size = size_ * max_dim / 100.0f;
  const float m = bokehDimension_ / pixel_size;

  const MemoryBuffer *image_input = inputs[IMAGE_INPUT_INDEX];
  const MemoryBuffer *bokeh_input = inputs[BOKEH_INPUT_INDEX];
  MemoryBuffer *bounding_input = inputs[BOUNDING_BOX_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({bounding_input}, area);
  const rcti &image_rect = image_input->get_rect();
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    const float bounding_box = *it.in(0);
    if (bounding_box <= 0.0f) {
      image_input->read_elem(x, y, it.out);
      continue;
    }

    float color_accum[4] = {0};
    float multiplier_accum[4] = {0};
    if (pixel_size < 2) {
      image_input->read_elem(x, y, color_accum);
      multiplier_accum[0] = 1.0f;
      multiplier_accum[1] = 1.0f;
      multiplier_accum[2] = 1.0f;
      multiplier_accum[3] = 1.0f;
    }
    const int miny = MAX2(y - pixel_size, image_rect.ymin);
    const int maxy = MIN2(y + pixel_size, image_rect.ymax);
    const int minx = MAX2(x - pixel_size, image_rect.xmin);
    const int maxx = MIN2(x + pixel_size, image_rect.xmax);
    const int step = getStep();
    const int elem_stride = image_input->elem_stride * step;
    const int row_stride = image_input->row_stride * step;
    const float *row_color = image_input->get_elem(minx, miny);
    for (int ny = miny; ny < maxy; ny += step, row_color += row_stride) {
      const float *color = row_color;
      const float v = bokehMidY_ - (ny - y) * m;
      for (int nx = minx; nx < maxx; nx += step, color += elem_stride) {
        const float u = bokehMidX_ - (nx - x) * m;
        float bokeh[4];
        bokeh_input->read_elem_checked(u, v, bokeh);
        madd_v4_v4v4(color_accum, bokeh, color);
        add_v4_v4(multiplier_accum, bokeh);
      }
    }
    it.out[0] = color_accum[0] * (1.0f / multiplier_accum[0]);
    it.out[1] = color_accum[1] * (1.0f / multiplier_accum[1]);
    it.out[2] = color_accum[2] * (1.0f / multiplier_accum[2]);
    it.out[3] = color_accum[3] * (1.0f / multiplier_accum[3]);
  }
}

}  // namespace blender::compositor
