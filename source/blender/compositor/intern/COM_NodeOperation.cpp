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

#include "COM_NodeOperation.h"
#include <typeinfo>
#include "COM_InputSocket.h"
#include "COM_SocketConnection.h"
#include "COM_defines.h"
#include "stdio.h"

NodeOperation::NodeOperation()
{
	this->resolutionInputSocketIndex = 0;
	this->complex = false;
	this->width = 0;
	this->height = 0;
	this->openCL = false;
}

void NodeOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	unsigned int temp[2];
	unsigned int temp2[2];
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	
	for (unsigned int index = 0 ; index < inputsockets.size();index++) {
		InputSocket *inputSocket = inputsockets[index];
		if (inputSocket->isConnected()) {
			if (index == this->resolutionInputSocketIndex) {
				inputSocket->determineResolution(resolution, preferredResolution);
				temp2[0] = resolution[0];
				temp2[1] = resolution[1];
				break;
			}
		}
	}
	for (unsigned int index = 0 ; index < inputsockets.size();index++) {
		InputSocket *inputSocket = inputsockets[index];
		if (inputSocket->isConnected()) {
			if (index != resolutionInputSocketIndex) {
				inputSocket->determineResolution(temp, temp2);
			}
		}
	}
}
void NodeOperation::setResolutionInputSocketIndex(unsigned int index)
{
	this->resolutionInputSocketIndex = index;
}
void NodeOperation::initExecution()
{
}

void NodeOperation::initMutex()
{
	BLI_mutex_init(&mutex);
}
void NodeOperation::deinitMutex()
{
	BLI_mutex_end(&mutex);
}
void NodeOperation::deinitExecution()
{
}
SocketReader *NodeOperation::getInputSocketReader(unsigned int inputSocketIndex)
{
	return this->getInputSocket(inputSocketIndex)->getReader();
}
NodeOperation *NodeOperation::getInputOperation(unsigned int inputSocketIndex)
{
	return this->getInputSocket(inputSocketIndex)->getOperation();
}

void NodeOperation::getConnectedInputSockets(vector<InputSocket*> *sockets)
{
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	for (vector<InputSocket*>::iterator iterator = inputsockets.begin() ; iterator!= inputsockets.end() ; iterator++) {
		InputSocket *socket = *iterator;
		if (socket->isConnected()) {
			sockets->push_back(socket);
		}
	}
}

bool NodeOperation::determineDependingAreaOfInterest(rcti * input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->isInputNode()) {
		BLI_init_rcti(output, input->xmin, input->xmax, input->ymin, input->ymax);
		return false;
	}
	else {
		unsigned int index;
		vector<InputSocket*> &inputsockets = this->getInputSockets();
	
		for (index = 0 ; index < inputsockets.size() ; index++) {
			InputSocket *inputsocket = inputsockets[index];
			if (inputsocket->isConnected()) {
				NodeOperation *inputoperation = (NodeOperation*)inputsocket->getConnection()->getFromNode();
				bool result = inputoperation->determineDependingAreaOfInterest(input, readOperation, output);
				if (result) {
					return true;
				}
			}
		}
		return false;
	}
}

cl_mem NodeOperation::COM_clAttachMemoryBufferToKernelParameter(cl_context context, cl_kernel kernel, int parameterIndex, int offsetIndex, list<cl_mem> *cleanup, MemoryBuffer **inputMemoryBuffers, SocketReader* reader) 
{
	cl_int error;
	MemoryBuffer* result = (MemoryBuffer*)reader->initializeTileData(NULL, inputMemoryBuffers);

	const cl_image_format imageFormat = {
		CL_RGBA,
		CL_FLOAT
	};

	cl_mem clBuffer = clCreateImage2D(context, CL_MEM_READ_ONLY|CL_MEM_USE_HOST_PTR, &imageFormat, result->getWidth(), 
	                                  result->getHeight(), 0, result->getBuffer(), &error);
	
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	if (error == CL_SUCCESS) cleanup->push_back(clBuffer);

	error = clSetKernelArg(kernel, parameterIndex, sizeof(cl_mem), &clBuffer);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	
	COM_clAttachMemoryBufferOffsetToKernelParameter(kernel, offsetIndex, result);
	return clBuffer;
}
	
void NodeOperation::COM_clAttachMemoryBufferOffsetToKernelParameter(cl_kernel kernel, int offsetIndex, MemoryBuffer *memoryBuffer) 
{
	if (offsetIndex != -1) {
		cl_int error;
		rcti* rect = memoryBuffer->getRect();
		cl_int2 offset = {rect->xmin, rect->ymin};

		error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	}
}

void NodeOperation::COM_clAttachSizeToKernelParameter(cl_kernel kernel, int offsetIndex) 
{
	if (offsetIndex != -1) {
		cl_int error;
		cl_int2 offset = {this->getWidth(), this->getHeight()};

		error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
	}
}

void NodeOperation::COM_clAttachOutputMemoryBufferToKernelParameter(cl_kernel kernel, int parameterIndex, cl_mem clOutputMemoryBuffer) 
{
	cl_int error;
	error = clSetKernelArg(kernel, parameterIndex, sizeof(cl_mem), &clOutputMemoryBuffer);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
}

void NodeOperation::COM_clEnqueueRange(cl_command_queue queue, cl_kernel kernel, MemoryBuffer *outputMemoryBuffer) {
	cl_int error;
	const size_t size[] = {outputMemoryBuffer->getWidth(),outputMemoryBuffer->getHeight()};
	
	error = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, size, 0, 0, 0, NULL);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
}

void NodeOperation::COM_clEnqueueRange(cl_command_queue queue, cl_kernel kernel, MemoryBuffer *outputMemoryBuffer, int offsetIndex) {
	cl_int error;
	const int width = outputMemoryBuffer->getWidth();
	const int height = outputMemoryBuffer->getHeight();
	int offsetx;
	int offsety;
	const int localSize = 32;
	size_t size[2];
	cl_int2 offset;
	
	for (offsety = 0 ; offsety < height; offsety+=localSize) {
		offset[1] = offsety;
		if (offsety+localSize < height) {
			size[1] = localSize;
		} else {
			size[1] = height - offsety;
		}
		for (offsetx = 0 ; offsetx < width ; offsetx+=localSize) {
			if (offsetx+localSize < width) {
				size[0] = localSize;
			} else {
				size[0] = width - offsetx;
			}
			offset[0] = offsetx;

			error = clSetKernelArg(kernel, offsetIndex, sizeof(cl_int2), &offset);
			if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
			error = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, size, 0, 0, 0, NULL);
			if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
			clFlush(queue);
		}
	}
}

cl_kernel NodeOperation::COM_clCreateKernel(cl_program program, const char *kernelname, list<cl_kernel> *clKernelsToCleanUp) 
{
	cl_int error;
	cl_kernel kernel = clCreateKernel(program, kernelname, &error)	;
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	
	}
	else {
		if (clKernelsToCleanUp) clKernelsToCleanUp->push_back(kernel);
	}
	return kernel;
	
}
