/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_WriteBufferOperation.h"
#include "COM_OpenCLDevice.h"

namespace blender::compositor {

WriteBufferOperation::WriteBufferOperation(DataType datatype)
{
  this->add_input_socket(datatype);
  memory_proxy_ = new MemoryProxy(datatype);
  memory_proxy_->set_write_buffer_operation(this);
  memory_proxy_->set_executor(nullptr);
  flags_.is_write_buffer_operation = true;
}
WriteBufferOperation::~WriteBufferOperation()
{
  if (memory_proxy_) {
    delete memory_proxy_;
    memory_proxy_ = nullptr;
  }
}

void WriteBufferOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  input_->read_sampled(output, x, y, sampler);
}

void WriteBufferOperation::init_execution()
{
  input_ = this->get_input_operation(0);
  memory_proxy_->allocate(this->get_width(), this->get_height());
}

void WriteBufferOperation::deinit_execution()
{
  input_ = nullptr;
  memory_proxy_->free();
}

void WriteBufferOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  MemoryBuffer *memory_buffer = memory_proxy_->get_buffer();
  float *buffer = memory_buffer->get_buffer();
  const uint8_t num_channels = memory_buffer->get_num_channels();
  if (input_->get_flags().complex) {
    void *data = input_->initialize_tile_data(rect);
    int x1 = rect->xmin;
    int y1 = rect->ymin;
    int x2 = rect->xmax;
    int y2 = rect->ymax;
    int x;
    int y;
    bool breaked = false;
    for (y = y1; y < y2 && (!breaked); y++) {
      int offset4 = (y * memory_buffer->get_width() + x1) * num_channels;
      for (x = x1; x < x2; x++) {
        input_->read(&(buffer[offset4]), x, y, data);
        offset4 += num_channels;
      }
      if (is_braked()) {
        breaked = true;
      }
    }
    if (data) {
      input_->deinitialize_tile_data(rect, data);
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
      int offset4 = (y * memory_buffer->get_width() + x1) * num_channels;
      for (x = x1; x < x2; x++) {
        input_->read_sampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
        offset4 += num_channels;
      }
      if (is_braked()) {
        breaked = true;
      }
    }
  }
}

void WriteBufferOperation::execute_opencl_region(OpenCLDevice *device,
                                                 rcti * /*rect*/,
                                                 uint /*chunk_number*/,
                                                 MemoryBuffer **input_memory_buffers,
                                                 MemoryBuffer *output_buffer)
{
  float *output_float_buffer = output_buffer->get_buffer();
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
  const uint output_buffer_width = output_buffer->get_width();
  const uint output_buffer_height = output_buffer->get_height();

  const cl_image_format *image_format = OpenCLDevice::determine_image_format(output_buffer);

  cl_mem cl_output_buffer = clCreateImage2D(device->get_context(),
                                            CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
                                            image_format,
                                            output_buffer_width,
                                            output_buffer_height,
                                            0,
                                            output_float_buffer,
                                            &error);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }

  /* STEP 2 */
  std::list<cl_mem> *cl_mem_to_clean_up = new std::list<cl_mem>();
  cl_mem_to_clean_up->push_back(cl_output_buffer);
  std::list<cl_kernel> *cl_kernels_to_clean_up = new std::list<cl_kernel>();

  input_->execute_opencl(device,
                         output_buffer,
                         cl_output_buffer,
                         input_memory_buffers,
                         cl_mem_to_clean_up,
                         cl_kernels_to_clean_up);

  /* STEP 3 */

  size_t origin[3] = {0, 0, 0};
  size_t region[3] = {output_buffer_width, output_buffer_height, 1};

  //  clFlush(queue);
  //  clFinish(queue);

  error = clEnqueueBarrier(device->get_queue());
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
  error = clEnqueueReadImage(device->get_queue(),
                             cl_output_buffer,
                             CL_TRUE,
                             origin,
                             region,
                             0,
                             0,
                             output_float_buffer,
                             0,
                             nullptr,
                             nullptr);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }

  this->get_memory_proxy()->get_buffer()->fill_from(*output_buffer);

  /* STEP 4 */
  while (!cl_mem_to_clean_up->empty()) {
    cl_mem mem = cl_mem_to_clean_up->front();
    error = clReleaseMemObject(mem);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
    cl_mem_to_clean_up->pop_front();
  }

  while (!cl_kernels_to_clean_up->empty()) {
    cl_kernel kernel = cl_kernels_to_clean_up->front();
    error = clReleaseKernel(kernel);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
    cl_kernels_to_clean_up->pop_front();
  }
  delete cl_kernels_to_clean_up;
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

void WriteBufferOperation::read_resolution_from_input_socket()
{
  NodeOperation *input_operation = this->get_input_operation(0);
  this->set_width(input_operation->get_width());
  this->set_height(input_operation->get_height());
}

}  // namespace blender::compositor
