/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_OpenCLDevice.h"
#include "COM_WorkScheduler.h"

typedef enum COM_VendorID  {NVIDIA = 0x10DE, AMD = 0x1002} COM_VendorID;

OpenCLDevice::OpenCLDevice(cl_context context, cl_device_id device, cl_program program, cl_int vendorId)
{
	this->m_device = device;
	this->m_context = context;
	this->m_program = program;
	this->m_queue = NULL;
	this->m_vendorID = vendorId;
}

bool OpenCLDevice::initialize()
{
	cl_int error;
	this->m_queue = clCreateCommandQueue(this->m_context, this->m_device, 0, &error);
	return false;
}

void OpenCLDevice::deinitialize()
{
	if (this->m_queue) {
		clReleaseCommandQueue(this->m_queue);
	}
}

void OpenCLDevice::execute(WorkPackage *work)
{
	const unsigned int chunkNumber = work->getChunkNumber();
	ExecutionGroup *executionGroup = work->getExecutionGroup();
	rcti rect;

	executionGroup->determineChunkRect(&rect, chunkNumber);
	MemoryBuffer **inputBuffers = executionGroup->getInputBuffersOpenCL(chunkNumber);
	MemoryBuffer *outputBuffer = executionGroup->allocateOutputBuffer(chunkNumber, &rect);

	executionGroup->getOutputOperation()->executeOpenCLRegion(this, &rect,
	                                                              chunkNumber, inputBuffers, outputBuffer);

	delete outputBuffer;
	
	executionGroup->finalizeChunkExecution(chunkNumber, inputBuffers);
}
cl_mem OpenCLDevice::COM_clAttachMemoryBufferToKernelParameter(cl_kernel kernel, int parameterIndex, int offsetIndex,
                                                               list<cl_mem> *cleanup, MemoryBuffer **inputMemoryBuffers,
                                                               SocketReader *reader)
{
	return COM_clAttachMemoryBufferToKernelParameter(kernel, parameterIndex, offsetIndex, cleanup, inputMemoryBuffers, (ReadBufferOperation *)reader);
}

cl_mem OpenCLDevice::COM_clAttachMemoryBufferToKernelParameter(cl_kernel kernel, int parameterIndex, int offsetIndex,
                                                               list<cl_mem> *cleanup, MemoryBuffer **inputMemoryBuffers,
                                                               ReadBufferOperation *reader)
{
	cl_int error;
	
	MemoryBuffer *result = reader->getInputMemoryBuffer(inputMemoryBuffers);

	const cl_image_format imageFormat = {
		CL_RGBA,
		CL_FLOAT
	};

	cl_mem clBuffer = clCreateImage2D(this->m_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &imageFormat, result->getWidth(),
	                                  result->getHeight(), 0, result->getBuffer(), &error);

	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	if (error == CL_SUCCESS) cleanup->push_back(clBuffer);

	error = clSetKernelArg(kernel, parameterIndex, sizeof(cl_mem), &clBuffer);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }

	COM_clAttachMemoryBufferOffsetToKernelParameter(kernel, offsetIndex, result);
	return clBuffer;
}

void OpenCLDevice::COM_clAttachMemoryBufferOffsetToKernelParameter(cl_kernel kernel, int offsetIndex, MemoryBuffer *memoryBuffer)
{
	if (offsetIndex != -1) {
		cl_int error;
		rcti *rect = memoryBuffer->getRect();
		cl_int2 offset = {rect->xmin, rect->ymin};

		error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	}
}

void OpenCLDevice::COM_clAttachSizeToKernelParameter(cl_kernel kernel, int offsetIndex, NodeOperation *operation)
{
	if (offsetIndex != -1) {
		cl_int error;
		cl_int2 offset = {(cl_int)operation->getWidth(), (cl_int)operation->getHeight()};

		error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	}
}

void OpenCLDevice::COM_clAttachOutputMemoryBufferToKernelParameter(cl_kernel kernel, int parameterIndex, cl_mem clOutputMemoryBuffer)
{
	cl_int error;
	error = clSetKernelArg(kernel, parameterIndex, sizeof(cl_mem), &clOutputMemoryBuffer);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
}

void OpenCLDevice::COM_clEnqueueRange(cl_kernel kernel, MemoryBuffer *outputMemoryBuffer)
{
	cl_int error;
	const size_t size[] = {(size_t)outputMemoryBuffer->getWidth(), (size_t)outputMemoryBuffer->getHeight()};

	error = clEnqueueNDRangeKernel(this->m_queue, kernel, 2, NULL, size, 0, 0, 0, NULL);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
}

void OpenCLDevice::COM_clEnqueueRange(cl_kernel kernel, MemoryBuffer *outputMemoryBuffer, int offsetIndex, NodeOperation *operation)
{
	cl_int error;
	const int width = outputMemoryBuffer->getWidth();
	const int height = outputMemoryBuffer->getHeight();
	int offsetx;
	int offsety;
	int localSize = 1024;
	size_t size[2];
	cl_int2 offset;

	if (this->m_vendorID == NVIDIA) {
		localSize = 32;
	}

	bool breaked = false;
	for (offsety = 0; offsety < height && (!breaked); offsety += localSize) {
		offset[1] = offsety;
		if (offsety + localSize < height) {
			size[1] = localSize;
		}
		else {
			size[1] = height - offsety;
		}

		for (offsetx = 0; offsetx < width && (!breaked); offsetx += localSize) {
			if (offsetx + localSize < width) {
				size[0] = localSize;
			}
			else {
				size[0] = width - offsetx;
			}
			offset[0] = offsetx;

			error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
			if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
			error = clEnqueueNDRangeKernel(this->m_queue, kernel, 2, NULL, size, 0, 0, 0, NULL);
			if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
			clFlush(this->m_queue);
			if (operation->isBreaked()) {
				breaked = false;
			}
		}
	}
}

cl_kernel OpenCLDevice::COM_clCreateKernel(const char *kernelname, list<cl_kernel> *clKernelsToCleanUp)
{
	cl_int error;
	cl_kernel kernel = clCreateKernel(this->m_program, kernelname, &error);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
	else {
		if (clKernelsToCleanUp) clKernelsToCleanUp->push_back(kernel);
	}
	return kernel;

}
