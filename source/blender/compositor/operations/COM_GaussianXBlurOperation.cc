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

#include "COM_GaussianXBlurOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

GaussianXBlurOperation::GaussianXBlurOperation() : GaussianBlurBaseOperation(eDimension::X)
{
}

void *GaussianXBlurOperation::initializeTileData(rcti * /*rect*/)
{
  lockMutex();
  if (!sizeavailable_) {
    updateGauss();
  }
  void *buffer = getInputOperation(0)->initializeTileData(nullptr);
  unlockMutex();
  return buffer;
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void GaussianXBlurOperation::initExecution()
{
  GaussianBlurBaseOperation::initExecution();

  initMutex();

  if (sizeavailable_ && execution_model_ == eExecutionModel::Tiled) {
    float rad = max_ff(size_ * data_.sizex, 0.0f);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    /* TODO(sergey): De-duplicate with the case below and Y blur. */
    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

/* TODO(manzanilla): to be removed with tiled implementation. */
void GaussianXBlurOperation::updateGauss()
{
  if (gausstab_ == nullptr) {
    updateSize();
    float rad = max_ff(size_ * data_.sizex, 0.0f);
    rad = min_ff(rad, MAX_GAUSSTAB_RADIUS);
    filtersize_ = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

    gausstab_ = BlurBaseOperation::make_gausstab(rad, filtersize_);
#ifdef BLI_HAVE_SSE2
    gausstab_sse_ = BlurBaseOperation::convert_gausstab_sse(gausstab_, filtersize_);
#endif
  }
}

void GaussianXBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  float ATTR_ALIGN(16) color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float multiplier_accum = 0.0f;
  MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
  const rcti &input_rect = inputBuffer->get_rect();
  float *buffer = inputBuffer->getBuffer();
  int bufferwidth = inputBuffer->getWidth();
  int bufferstartx = input_rect.xmin;
  int bufferstarty = input_rect.ymin;

  int xmin = max_ii(x - filtersize_, input_rect.xmin);
  int xmax = min_ii(x + filtersize_ + 1, input_rect.xmax);
  int ymin = max_ii(y, input_rect.ymin);

  int step = getStep();
  int offsetadd = getOffsetAdd();
  int bufferindex = ((xmin - bufferstartx) * 4) + ((ymin - bufferstarty) * 4 * bufferwidth);

#ifdef BLI_HAVE_SSE2
  __m128 accum_r = _mm_load_ps(color_accum);
  for (int nx = xmin, index = (xmin - x) + filtersize_; nx < xmax; nx += step, index += step) {
    __m128 reg_a = _mm_load_ps(&buffer[bufferindex]);
    reg_a = _mm_mul_ps(reg_a, gausstab_sse_[index]);
    accum_r = _mm_add_ps(accum_r, reg_a);
    multiplier_accum += gausstab_[index];
    bufferindex += offsetadd;
  }
  _mm_store_ps(color_accum, accum_r);
#else
  for (int nx = xmin, index = (xmin - x) + filtersize_; nx < xmax; nx += step, index += step) {
    const float multiplier = gausstab_[index];
    madd_v4_v4fl(color_accum, &buffer[bufferindex], multiplier);
    multiplier_accum += multiplier;
    bufferindex += offsetadd;
  }
#endif
  mul_v4_v4fl(output, color_accum, 1.0f / multiplier_accum);
}

void GaussianXBlurOperation::executeOpenCL(OpenCLDevice *device,
                                           MemoryBuffer *outputMemoryBuffer,
                                           cl_mem clOutputBuffer,
                                           MemoryBuffer **inputMemoryBuffers,
                                           std::list<cl_mem> *clMemToCleanUp,
                                           std::list<cl_kernel> * /*clKernelsToCleanUp*/)
{
  cl_kernel gaussianXBlurOperationKernel = device->COM_clCreateKernel(
      "gaussianXBlurOperationKernel", nullptr);
  cl_int filter_size = filtersize_;

  cl_mem gausstab = clCreateBuffer(device->getContext(),
                                   CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   sizeof(float) * (filtersize_ * 2 + 1),
                                   gausstab_,
                                   nullptr);

  device->COM_clAttachMemoryBufferToKernelParameter(
      gaussianXBlurOperationKernel, 0, 1, clMemToCleanUp, inputMemoryBuffers, inputProgram_);
  device->COM_clAttachOutputMemoryBufferToKernelParameter(
      gaussianXBlurOperationKernel, 2, clOutputBuffer);
  device->COM_clAttachMemoryBufferOffsetToKernelParameter(
      gaussianXBlurOperationKernel, 3, outputMemoryBuffer);
  clSetKernelArg(gaussianXBlurOperationKernel, 4, sizeof(cl_int), &filter_size);
  device->COM_clAttachSizeToKernelParameter(gaussianXBlurOperationKernel, 5, this);
  clSetKernelArg(gaussianXBlurOperationKernel, 6, sizeof(cl_mem), &gausstab);

  device->COM_clEnqueueRange(gaussianXBlurOperationKernel, outputMemoryBuffer, 7, this);

  clReleaseMemObject(gausstab);
}

void GaussianXBlurOperation::deinitExecution()
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

bool GaussianXBlurOperation::determineDependingAreaOfInterest(rcti *input,
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
      newInput.xmax = input->xmax + filtersize_ + 1;
      newInput.xmin = input->xmin - filtersize_ - 1;
      newInput.ymax = input->ymax;
      newInput.ymin = input->ymin;
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
