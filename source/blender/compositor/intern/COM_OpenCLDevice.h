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

#include "COM_Device.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WorkScheduler.h"
#include "clew.h"

namespace blender::compositor {

/**
 * \brief device representing an GPU OpenCL device.
 * an instance of this class represents a single cl_device
 */
class OpenCLDevice : public Device {
 private:
  /**
   * \brief opencl context
   */
  cl_context m_context;

  /**
   * \brief opencl device
   */
  cl_device_id m_device;

  /**
   * \brief opencl program
   */
  cl_program m_program;

  /**
   * \brief opencl command queue
   */
  cl_command_queue m_queue;

  /**
   * \brief opencl vendor ID
   */
  cl_int m_vendorID;

 public:
  /**
   * \brief constructor with opencl device
   * \param context:
   * \param device:
   * \param program:
   * \param vendorID:
   */
  OpenCLDevice(cl_context context, cl_device_id device, cl_program program, cl_int vendorId);

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
  static const cl_image_format *determineImageFormat(MemoryBuffer *memoryBuffer);

  cl_context getContext()
  {
    return this->m_context;
  }

  cl_command_queue getQueue()
  {
    return this->m_queue;
  }

  cl_mem COM_clAttachMemoryBufferToKernelParameter(cl_kernel kernel,
                                                   int parameterIndex,
                                                   int offsetIndex,
                                                   std::list<cl_mem> *cleanup,
                                                   MemoryBuffer **inputMemoryBuffers,
                                                   SocketReader *reader);
  cl_mem COM_clAttachMemoryBufferToKernelParameter(cl_kernel kernel,
                                                   int parameterIndex,
                                                   int offsetIndex,
                                                   std::list<cl_mem> *cleanup,
                                                   MemoryBuffer **inputMemoryBuffers,
                                                   ReadBufferOperation *reader);
  void COM_clAttachMemoryBufferOffsetToKernelParameter(cl_kernel kernel,
                                                       int offsetIndex,
                                                       MemoryBuffer *memoryBuffers);
  void COM_clAttachOutputMemoryBufferToKernelParameter(cl_kernel kernel,
                                                       int parameterIndex,
                                                       cl_mem clOutputMemoryBuffer);
  void COM_clAttachSizeToKernelParameter(cl_kernel kernel,
                                         int offsetIndex,
                                         NodeOperation *operation);
  void COM_clEnqueueRange(cl_kernel kernel, MemoryBuffer *outputMemoryBuffer);
  void COM_clEnqueueRange(cl_kernel kernel,
                          MemoryBuffer *outputMemoryBuffer,
                          int offsetIndex,
                          NodeOperation *operation);
  cl_kernel COM_clCreateKernel(const char *kernelname, std::list<cl_kernel> *clKernelsToCleanUp);
};

}  // namespace blender::compositor
