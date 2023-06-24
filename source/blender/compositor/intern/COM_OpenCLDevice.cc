/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_OpenCLDevice.h"

#include "COM_ExecutionGroup.h"
#include "COM_ReadBufferOperation.h"

namespace blender::compositor {

enum COM_VendorID { NVIDIA = 0x10DE, AMD = 0x1002 };
const cl_image_format IMAGE_FORMAT_COLOR = {
    CL_RGBA,
    CL_FLOAT,
};
const cl_image_format IMAGE_FORMAT_VECTOR = {
    CL_RGB,
    CL_FLOAT,
};
const cl_image_format IMAGE_FORMAT_VALUE = {
    CL_R,
    CL_FLOAT,
};

OpenCLDevice::OpenCLDevice(cl_context context,
                           cl_device_id device,
                           cl_program program,
                           cl_int vendor_id)
{
  device_ = device;
  context_ = context;
  program_ = program;
  queue_ = nullptr;
  vendor_id_ = vendor_id;

  cl_int error;
  queue_ = clCreateCommandQueue(context_, device_, 0, &error);
}

OpenCLDevice::OpenCLDevice(OpenCLDevice &&other) noexcept
    : context_(other.context_),
      device_(other.device_),
      program_(other.program_),
      queue_(other.queue_),
      vendor_id_(other.vendor_id_)
{
  other.queue_ = nullptr;
}

OpenCLDevice::~OpenCLDevice()
{
  if (queue_) {
    clReleaseCommandQueue(queue_);
  }
}

void OpenCLDevice::execute(WorkPackage *work_package)
{
  const uint chunk_number = work_package->chunk_number;
  ExecutionGroup *execution_group = work_package->execution_group;

  MemoryBuffer **input_buffers = execution_group->get_input_buffers_opencl(chunk_number);
  MemoryBuffer *output_buffer = execution_group->allocate_output_buffer(work_package->rect);

  execution_group->get_output_operation()->execute_opencl_region(
      this, &work_package->rect, chunk_number, input_buffers, output_buffer);

  delete output_buffer;

  execution_group->finalize_chunk_execution(chunk_number, input_buffers);
}
cl_mem OpenCLDevice::COM_cl_attach_memory_buffer_to_kernel_parameter(
    cl_kernel kernel,
    int parameter_index,
    int offset_index,
    std::list<cl_mem> *cleanup,
    MemoryBuffer **input_memory_buffers,
    SocketReader *reader)
{
  return COM_cl_attach_memory_buffer_to_kernel_parameter(kernel,
                                                         parameter_index,
                                                         offset_index,
                                                         cleanup,
                                                         input_memory_buffers,
                                                         (ReadBufferOperation *)reader);
}

const cl_image_format *OpenCLDevice::determine_image_format(MemoryBuffer *memory_buffer)
{
  switch (memory_buffer->get_num_channels()) {
    case 1:
      return &IMAGE_FORMAT_VALUE;
      break;
    case 3:
      return &IMAGE_FORMAT_VECTOR;
      break;
    case 4:
      return &IMAGE_FORMAT_COLOR;
      break;
    default:
      BLI_assert_msg(0, "Unsupported num_channels.");
  }

  return &IMAGE_FORMAT_COLOR;
}

cl_mem OpenCLDevice::COM_cl_attach_memory_buffer_to_kernel_parameter(
    cl_kernel kernel,
    int parameter_index,
    int offset_index,
    std::list<cl_mem> *cleanup,
    MemoryBuffer **input_memory_buffers,
    ReadBufferOperation *reader)
{
  cl_int error;

  MemoryBuffer *result = reader->get_input_memory_buffer(input_memory_buffers);

  const cl_image_format *image_format = determine_image_format(result);

  cl_mem cl_buffer = clCreateImage2D(context_,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     image_format,
                                     result->get_width(),
                                     result->get_height(),
                                     0,
                                     result->get_buffer(),
                                     &error);

  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
  if (error == CL_SUCCESS) {
    cleanup->push_back(cl_buffer);
  }

  error = clSetKernelArg(kernel, parameter_index, sizeof(cl_mem), &cl_buffer);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }

  COM_cl_attach_memory_buffer_offset_to_kernel_parameter(kernel, offset_index, result);
  return cl_buffer;
}

void OpenCLDevice::COM_cl_attach_memory_buffer_offset_to_kernel_parameter(
    cl_kernel kernel, int offset_index, MemoryBuffer *memory_buffer)
{
  if (offset_index != -1) {
    cl_int error;
    const rcti &rect = memory_buffer->get_rect();
    cl_int2 offset = {{rect.xmin, rect.ymin}};

    error = clSetKernelArg(kernel, offset_index, sizeof(cl_int2), &offset);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
  }
}

void OpenCLDevice::COM_cl_attach_size_to_kernel_parameter(cl_kernel kernel,
                                                          int offset_index,
                                                          NodeOperation *operation)
{
  if (offset_index != -1) {
    cl_int error;
    cl_int2 offset = {{(cl_int)operation->get_width(), (cl_int)operation->get_height()}};

    error = clSetKernelArg(kernel, offset_index, sizeof(cl_int2), &offset);
    if (error != CL_SUCCESS) {
      printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
    }
  }
}

void OpenCLDevice::COM_cl_attach_output_memory_buffer_to_kernel_parameter(
    cl_kernel kernel, int parameter_index, cl_mem cl_output_memory_buffer)
{
  cl_int error;
  error = clSetKernelArg(kernel, parameter_index, sizeof(cl_mem), &cl_output_memory_buffer);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
}

void OpenCLDevice::COM_cl_enqueue_range(cl_kernel kernel, MemoryBuffer *output_memory_buffer)
{
  cl_int error;
  const size_t size[] = {
      size_t(output_memory_buffer->get_width()),
      size_t(output_memory_buffer->get_height()),
  };

  error = clEnqueueNDRangeKernel(queue_, kernel, 2, nullptr, size, nullptr, 0, nullptr, nullptr);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
}

void OpenCLDevice::COM_cl_enqueue_range(cl_kernel kernel,
                                        MemoryBuffer *output_memory_buffer,
                                        int offset_index,
                                        NodeOperation *operation)
{
  cl_int error;
  const int width = output_memory_buffer->get_width();
  const int height = output_memory_buffer->get_height();
  int offsetx;
  int offsety;
  int local_size = 1024;
  size_t size[2];
  cl_int2 offset;

  if (vendor_id_ == NVIDIA) {
    local_size = 32;
  }

  bool breaked = false;
  for (offsety = 0; offsety < height && (!breaked); offsety += local_size) {
    offset.s[1] = offsety;
    if (offsety + local_size < height) {
      size[1] = local_size;
    }
    else {
      size[1] = height - offsety;
    }

    for (offsetx = 0; offsetx < width && (!breaked); offsetx += local_size) {
      if (offsetx + local_size < width) {
        size[0] = local_size;
      }
      else {
        size[0] = width - offsetx;
      }
      offset.s[0] = offsetx;

      error = clSetKernelArg(kernel, offset_index, sizeof(cl_int2), &offset);
      if (error != CL_SUCCESS) {
        printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
      }
      error = clEnqueueNDRangeKernel(
          queue_, kernel, 2, nullptr, size, nullptr, 0, nullptr, nullptr);
      if (error != CL_SUCCESS) {
        printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
      }
      clFlush(queue_);
      if (operation->is_braked()) {
        breaked = false;
      }
    }
  }
}

cl_kernel OpenCLDevice::COM_cl_create_kernel(const char *kernelname,
                                             std::list<cl_kernel> *cl_kernels_to_clean_up)
{
  cl_int error;
  cl_kernel kernel = clCreateKernel(program_, kernelname, &error);
  if (error != CL_SUCCESS) {
    printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
  }
  else {
    if (cl_kernels_to_clean_up) {
      cl_kernels_to_clean_up->push_back(kernel);
    }
  }
  return kernel;
}

}  // namespace blender::compositor
