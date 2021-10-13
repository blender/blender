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

#include "COM_WriteBufferOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

WriteBufferOperation::WriteBufferOperation(DataType datatype)
{
  this->addInputSocket(datatype);
  memoryProxy_ = new MemoryProxy(datatype);
  memoryProxy_->setWriteBufferOperation(this);
  memoryProxy_->setExecutor(nullptr);
  flags.is_write_buffer_operation = true;
}
WriteBufferOperation::~WriteBufferOperation()
{
  if (memoryProxy_) {
    delete memoryProxy_;
    memoryProxy_ = nullptr;
  }
}

void WriteBufferOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  input_->readSampled(output, x, y, sampler);
}

void WriteBufferOperation::initExecution()
{
  input_ = this->getInputOperation(0);
  memoryProxy_->allocate(this->getWidth(), this->getHeight());
}

void WriteBufferOperation::deinitExecution()
{
  input_ = nullptr;
  memoryProxy_->free();
}

void WriteBufferOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  MemoryBuffer *memoryBuffer = memoryProxy_->getBuffer();
  float *buffer = memoryBuffer->getBuffer();
  const uint8_t num_channels = memoryBuffer->get_num_channels();
  if (input_->get_flags().complex) {
    void *data = input_->initializeTileData(rect);
    int x1 = rect->xmin;
    int y1 = rect->ymin;
    int x2 = rect->xmax;
    int y2 = rect->ymax;
    int x;
    int y;
    bool breaked = false;
    for (y = y1; y < y2 && (!breaked); y++) {
      int offset4 = (y * memoryBuffer->getWidth() + x1) * num_channels;
      for (x = x1; x < x2; x++) {
        input_->read(&(buffer[offset4]), x, y, data);
        offset4 += num_channels;
      }
      if (isBraked()) {
        breaked = true;
      }
    }
    if (data) {
      input_->deinitializeTileData(rect, data);
      data = nullptr;
    }
  }
  else {
    int x1 = rect->xmin;
    int y1 = rect->ymin;
    int x2 = rect->xmax;
    int y2 = rect->ymax;

    int x;
    int y;
    bool breaked = false;
    for (y = y1; y < y2 && (!breaked); y++) {
      int offset4 = (y * memoryBuffer->getWidth() + x1) * num_channels;
      for (x = x1; x < x2; x++) {
        input_->readSampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
        offset4 += num_channels;
      }
      if (isBraked()) {
        breaked = true;
      }
    }
  }
}

void WriteBufferOperation::executeOpenCLRegion(OpenCLDevice *device,
                                               rcti * /*rect*/,
                                               unsigned int /*chunkNumber*/,
                                               MemoryBuffer **inputMemoryBuffers,
                                               MemoryBuffer *outputBuffer)
{
  float *outputFloatBuffer = outputBuffer->getBuffer();
  cl_int error;
  /*
   * 1. create cl_mem from outputbuffer
   * 2. call NodeOperation (input) executeOpenCLChunk(.....)
   * 3. schedule read back from opencl to main device (outputbuffer)
   * 4. schedule native callback
   *
   * NOTE: list of cl_mem will be filled by 2, and needs to be cleaned up by 4
   */
  /* STEP 1 */
  const unsigned int outputBufferWidth = outputBuffer->getWidth();
  const unsigned int outputBufferHeight = outputBuffer->getHeight();

  const cl_image_format *imageFormat = OpenCLDevice::determineImageFormat(outputBuffer);

  cl_mem clOutputBuffer = clCreateImage2D(device->getContext(),
                                          CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
                                          imageFormat,
                                          outputBufferWidth,
                                          outputBufferHeight,
                                          0,
                                          outputFloatBuffer,
                                          &error);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }

  /* STEP 2 */
  std::list<cl_mem> *clMemToCleanUp = new std::list<cl_mem>();
  clMemToCleanUp->push_back(clOutputBuffer);
  std::list<cl_kernel> *clKernelsToCleanUp = new std::list<cl_kernel>();

  input_->executeOpenCL(device,
                        outputBuffer,
                        clOutputBuffer,
                        inputMemoryBuffers,
                        clMemToCleanUp,
                        clKernelsToCleanUp);

  /* STEP 3 */

  size_t origin[3] = {0, 0, 0};
  size_t region[3] = {outputBufferWidth, outputBufferHeight, 1};

  //  clFlush(queue);
  //  clFinish(queue);

  error = clEnqueueBarrier(device->getQueue());
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
  error = clEnqueueReadImage(device->getQueue(),
                             clOutputBuffer,
                             CL_TRUE,
                             origin,
                             region,
                             0,
                             0,
                             outputFloatBuffer,
                             0,
                             nullptr,
                             nullptr);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }

  this->getMemoryProxy()->getBuffer()->fill_from(*outputBuffer);

  /* STEP 4 */
  while (!clMemToCleanUp->empty()) {
    cl_mem mem = clMemToCleanUp->front();
    error = clReleaseMemObject(mem);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
    clMemToCleanUp->pop_front();
  }

  while (!clKernelsToCleanUp->empty()) {
    cl_kernel kernel = clKernelsToCleanUp->front();
    error = clReleaseKernel(kernel);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
    clKernelsToCleanUp->pop_front();
  }
  delete clKernelsToCleanUp;
}

void WriteBufferOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  /* make sure there is at least one pixel stored in case the input is a single value */
  single_value_ = false;
  if (BLI_rcti_size_x(&r_area) == 0) {
    r_area.xmax += 1;
    single_value_ = true;
  }
  if (BLI_rcti_size_y(&r_area) == 0) {
    r_area.ymax += 1;
    single_value_ = true;
  }
}

void WriteBufferOperation::readResolutionFromInputSocket()
{
  NodeOperation *inputOperation = this->getInputOperation(0);
  this->setWidth(inputOperation->getWidth());
  this->setHeight(inputOperation->getHeight());
}

}  // namespace blender::compositor
