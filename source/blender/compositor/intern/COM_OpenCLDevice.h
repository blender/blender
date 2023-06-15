/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
   * \brief OPENCL context
   */
  cl_context context_;

  /**
   * \brief OPENCL device
   */
  cl_device_id device_;

  /**
   * \brief OPENCL program
   */
  cl_program program_;

  /**
   * \brief OPENCL command queue
   */
  cl_command_queue queue_;

  /**
   * \brief OPENCL vendor ID
   */
  cl_int vendor_id_;

 public:
  /**
   * \brief constructor with OPENCL device
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
