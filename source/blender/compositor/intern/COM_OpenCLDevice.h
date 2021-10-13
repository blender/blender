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

class OpenCLDevice;

#pragma once

#include <list>

#include "COM_Device.h"

#include "clew.h"

namespace blender::compositor {

class NodeOperation;
class MemoryBuffer;
class ReadBufferOperation;

typedef NodeOperation SocketReader;

/**
 * \brief device representing an GPU OpenCL device.
 * an instance of this class represents a single cl_device
 */
class OpenCLDevice : public Device {
 private:
  /**
   * \brief opencl context
   */
  cl_context context_;

  /**
   * \brief opencl device
   */
  cl_device_id device_;

  /**
   * \brief opencl program
   */
  cl_program program_;

  /**
   * \brief opencl command queue
   */
  cl_command_queue queue_;

  /**
   * \brief opencl vendor ID
   */
  cl_int vendor_id_;

 public:
  /**
   * \brief constructor with opencl device
   * \param context:
   * \param device:
   * \param program:
   * \param vendorID:
   */
  OpenCLDevice(cl_context context, cl_device_id device, cl_program program, cl_int vendor_id);

  OpenCLDevice(OpenCLDevice &&other) noexcept;

  ~OpenCLDevice();

  /**
   * \brief execute a WorkPackage
   * \param work: the WorkPackage to execute
   */
  void execute(WorkPackage *work) override;

  /**
   * \brief determine an image format
   * \param memorybuffer:
   */
  static const cl_image_format *determine_image_format(MemoryBuffer *memory_buffer);

  cl_context get_context()
  {
    return context_;
  }

  cl_command_queue get_queue()
  {
    return queue_;
  }

  cl_mem COM_cl_attach_memory_buffer_to_kernel_parameter(cl_kernel kernel,
                                                         int parameter_index,
                                                         int offset_index,
                                                         std::list<cl_mem> *cleanup,
                                                         MemoryBuffer **input_memory_buffers,
                                                         SocketReader *reader);
  cl_mem COM_cl_attach_memory_buffer_to_kernel_parameter(cl_kernel kernel,
                                                         int parameter_index,
                                                         int offset_index,
                                                         std::list<cl_mem> *cleanup,
                                                         MemoryBuffer **input_memory_buffers,
                                                         ReadBufferOperation *reader);
  void COM_cl_attach_memory_buffer_offset_to_kernel_parameter(cl_kernel kernel,
                                                              int offset_index,
                                                              MemoryBuffer *memory_buffers);
  void COM_cl_attach_output_memory_buffer_to_kernel_parameter(cl_kernel kernel,
                                                              int parameter_index,
                                                              cl_mem cl_output_memory_buffer);
  void COM_cl_attach_size_to_kernel_parameter(cl_kernel kernel,
                                              int offset_index,
                                              NodeOperation *operation);
  void COM_cl_enqueue_range(cl_kernel kernel, MemoryBuffer *output_memory_buffer);
  void COM_cl_enqueue_range(cl_kernel kernel,
                            MemoryBuffer *output_memory_buffer,
                            int offset_index,
                            NodeOperation *operation);
  cl_kernel COM_cl_create_kernel(const char *kernelname,
                                 std::list<cl_kernel> *cl_kernels_to_clean_up);
};

}  // namespace blender::compositor
