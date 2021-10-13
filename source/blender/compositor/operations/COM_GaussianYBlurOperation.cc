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

#include "COM_GaussianYBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

GaussianYBlurOperation::GaussianYBlurOperation() : GaussianBlurBaseOperation(eDimension::Y)
{
}

void *GaussianYBlurOperation::initializeTileData(rcti * /*rect*/)
{
  lockMutex();
  if (!sizeavailable_) {
    updateGauss();
  }
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  unlockMutex();
  return buffer;
}

void GaussianYBlurOperation::initExecution()
{
  GaussianBlurBaseOperation::initExecution();

  initMutex();

  if (sizeavailable_ && execution_model_ == eExecutionModel::Tiled) {
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianYBlurOperation::updateGauss()
{
  if (gausstab_ == nullptr) {
    updateSize();
    float rad = max_ff(size_ * data_.sizey, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianYBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  float ATTR_ALIGN(16) color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float multiplier_accum = 0.0f;
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  const rcti &input_rect = inputBuffer->get_rect();
  float *buffer = inputBuffer->getBuffer();
  int bufferwidth = inputBuffer->getWidth();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int xmin = max_ii(x, input_rect.xmin);
  int ymin = max_ii(y - filtersize_, input_rect.ymin);
  int ymax = min_ii(y + filtersize_ + 1, input_rect.ymax);

  int index;
  int step = getStep();
  const int bufferIndexx = ((xmin - bufferstartx) * 4);

#ifdef BLI_HAVE_SSE2
  __m128 accum_r = _mm_load_ps(color_accum);
  for (int ny = ymin; ny < ymax; ny += step) {
    index = (ny - y) + filtersize_;
    int bufferindex = bufferIndexx + ((ny - bufferstarty) * 4 * bufferwidth);
    const float multiplier = gausstab_[index];
    __m128 reg_a = _mm_load_ps(&buffer[bufferindex]);
    reg_a = _mm_mul_ps(reg_a, gausstab_sse_[index]);
    accum_r = _mm_add_ps(accum_r, reg_a);
    multiplier_accum += multiplier;
  }
  _mm_store_ps(color_accum, accum_r);
#else
  for (int ny = ymin; ny < ymax; ny += step) {
    index = (ny - y) + filtersize_;
    int bufferindex = bufferIndexx + ((ny - bufferstarty) * 4 * bufferwidth);
    const float multiplier = gausstab_[index];
    madd_v4_v4fl(color_accum, &buffer[bufferindex], multiplier);
    multiplier_accum += multiplier;
  }
#endif
  mul_v4_v4fl(output, color_accum, 1.0f / multiplier_accum);
}

void GaussianYBlurOperation::executeOpenCL(OpenCLDevice *device,
                                           MemoryBuffer *outputMemoryBuffer,
                                           cl_mem clOutputBuffer,
                                           MemoryBuffer **inputMemoryBuffers,
                                           std::list<cl_mem> *clMemToCleanUp,
                                           std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel gaussianYBlurOperationKernel = device->COM_clCreateKernel(
      "gaussianYBlurOperationKernel", nullptr);
  cl_int filter_size = filtersize_;

  cl_mem gausstab = clCreateBuffer(device->getContext(),
                                   CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   sizeof(float) * (filtersize_ * 2 + 1),
                                   gausstab_,
                                   nullptr);

  device->COM_clAttachMemoryBufferToKernelParameter(
      gaussianYBlurOperationKernel, 0, 1, clMemToCleanUp, inputMemoryBuffers, inputProgram_);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(
      gaussianYBlurOperationKernel, 2, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(
      gaussianYBlurOperationKernel, 3, outputMemoryBuffer);
  clSetKernelArg(gaussianYBlurOperationKernel, 4, sizeof(cl_int), &filter_size);
  device->COM_clAttachSizeToKernelParameter(gaussianYBlurOperationKernel, 5, this);
  clSetKernelArg(gaussianYBlurOperationKernel, 6, sizeof(cl_mem), &gausstab);

  device->COM_clEnqueueRange(gaussianYBlurOperationKernel, outputMemoryBuffer, 7, this);

  clReleaseMemObject(gausstab);
}

void GaussianYBlurOperation::deinitExecution()
{
  GaussianBlurBaseOperation::deinitExecution();

  if (gausstab_) {
    MEM_freeN(gausstab_);
    gausstab_ = nullptr;
  }
#ifdef BLI_HAVE_SSE2
  if (gausstab_sse_) {
    MEM_freeN(gausstab_sse_);
    gausstab_sse_ = nullptr;
  }
#endif

  deinitMutex();
}

bool GaussianYBlurOperation::determineDependingAreaOfInterest(rcti *input,
                                                              ReadBufferOperation *readOperation,
                                                              rcti *output)
{
  rcti newInput;

  if (!sizeavailable_) {
    rcti sizeInput;
    sizeInput.xmin = 0;
    sizeInput.ymin = 0;
    sizeInput.xmax = 5;
    sizeInput.ymax = 5;
    NodeOperation *operation = this->getInputOperation(1);
    if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
      return true;
    }
  }
  {
    if (sizeavailable_ && gausstab_ != nullptr) {
      newInput.xmax = input->xmax;
      newInput.xmin = input->xmin;
      newInput.ymax = input->ymax + filtersize_ + 1;
      newInput.ymin = input->ymin - filtersize_ - 1;
    }
    else {
      newInput.xmax = this->getWidth();
      newInput.xmin = 0;
      newInput.ymax = this->getHeight();
      newInput.ymin = 0;
    }
    return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
  }
}

}  // namespace blender::compositor
