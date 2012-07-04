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

#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"
#include <stdio.h>
#include "COM_OpenCLDevice.h"

WriteBufferOperation::WriteBufferOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->m_memoryProxy = new MemoryProxy();
	this->m_memoryProxy->setWriteBufferOperation(this);
	this->m_memoryProxy->setExecutor(NULL);
}
WriteBufferOperation::~WriteBufferOperation()
{
	if (this->m_memoryProxy) {
		delete this->m_memoryProxy;
		this->m_memoryProxy = NULL;
	}
}

void WriteBufferOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	this->m_input->read(color, x, y, sampler, inputBuffers);
}

void WriteBufferOperation::initExecution()
{
	this->m_input = this->getInputOperation(0);
	this->m_memoryProxy->allocate(this->m_width, this->m_height);
}

void WriteBufferOperation::deinitExecution()
{
	this->m_input = NULL;
	this->m_memoryProxy->free();
}

void WriteBufferOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers)
{
	//MemoryBuffer *memoryBuffer = MemoryManager::getMemoryBuffer(this->getMemoryProxy(), tileNumber);
	MemoryBuffer *memoryBuffer = this->m_memoryProxy->getBuffer();
	float *buffer = memoryBuffer->getBuffer();
	if (this->m_input->isComplex()) {
		bNode* bnode = this->m_input->getbNode();
//		if (bnode&& bnode->new_node) bnode->new_node->highlight++;

		void *data = this->m_input->initializeTileData(rect, memoryBuffers);
		int x1 = rect->xmin;
		int y1 = rect->ymin;
		int x2 = rect->xmax;
		int y2 = rect->ymax;
		int x;
		int y;
		bool breaked = false;
		for (y = y1; y < y2 && (!breaked); y++) {
			int offset4 = (y * memoryBuffer->getWidth() + x1) * COM_NUMBER_OF_CHANNELS;
			for (x = x1; x < x2; x++) {
				this->m_input->read(&(buffer[offset4]), x, y, memoryBuffers, data);
				offset4 += COM_NUMBER_OF_CHANNELS;

			}
			if (isBreaked()) {
				breaked = true;
			}

		}
		if (data) {
			this->m_input->deinitializeTileData(rect, memoryBuffers, data);
			data = NULL;
		}
//		if (bnode&& bnode->new_node) bnode->new_node->highlight++;
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
			int offset4 = (y * memoryBuffer->getWidth() + x1) * COM_NUMBER_OF_CHANNELS;
			for (x = x1; x < x2; x++) {
				this->m_input->read(&(buffer[offset4]), x, y, COM_PS_NEAREST, memoryBuffers);
				offset4 += COM_NUMBER_OF_CHANNELS;
			}
			if (isBreaked()) {
				breaked = true;
			}
		}
	}
	memoryBuffer->setCreatedState();
}

void WriteBufferOperation::executeOpenCLRegion(OpenCLDevice* device, rcti *rect, unsigned int chunkNumber, MemoryBuffer **inputMemoryBuffers, MemoryBuffer *outputBuffer)
{
	float *outputFloatBuffer = outputBuffer->getBuffer();
	cl_int error;
	/*
	 * 1. create cl_mem from outputbuffer
	 * 2. call NodeOperation (input) executeOpenCLChunk(.....)
	 * 3. schedule readback from opencl to main device (outputbuffer)
	 * 4. schedule native callback
	 *
	 * note: list of cl_mem will be filled by 2, and needs to be cleaned up by 4
	 */
	// STEP 1
	const unsigned int outputBufferWidth = outputBuffer->getWidth();
	const unsigned int outputBufferHeight = outputBuffer->getHeight();

	const cl_image_format imageFormat = {
		CL_RGBA,
		CL_FLOAT
	};

	cl_mem clOutputBuffer = clCreateImage2D(device->getContext(), CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, &imageFormat, outputBufferWidth, outputBufferHeight, 0, outputFloatBuffer, &error);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	
	// STEP 2
	list<cl_mem> *clMemToCleanUp = new list<cl_mem>();
	clMemToCleanUp->push_back(clOutputBuffer);
	list<cl_kernel> *clKernelsToCleanUp = new list<cl_kernel>();
	bNode* bnode = this->m_input->getbNode();
//	if (bnode&& bnode->new_node) bnode->new_node->highlight++;

	this->m_input->executeOpenCL(device, outputBuffer, clOutputBuffer, inputMemoryBuffers, clMemToCleanUp, clKernelsToCleanUp);

	// STEP 3

	size_t origin[3] = {0, 0, 0};
	size_t region[3] = {outputBufferWidth, outputBufferHeight, 1};

//	clFlush(queue);
//	clFinish(queue);

	error = clEnqueueBarrier(device->getQueue());
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	error = clEnqueueReadImage(device->getQueue(), clOutputBuffer, CL_TRUE, origin, region, 0, 0, outputFloatBuffer, 0, NULL, NULL);
	if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
	
	this->getMemoryProxy()->getBuffer()->copyContentFrom(outputBuffer);

//	if (bnode&& bnode->new_node) bnode->new_node->highlight++;
	// STEP 4

	
	while (clMemToCleanUp->size() > 0) {
		cl_mem mem = clMemToCleanUp->front();
		error = clReleaseMemObject(mem);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
		clMemToCleanUp->pop_front();
	}

	while (clKernelsToCleanUp->size() > 0) {
		cl_kernel kernel = clKernelsToCleanUp->front();
		error = clReleaseKernel(kernel);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
		clKernelsToCleanUp->pop_front();
	}
	delete clKernelsToCleanUp;
}

void WriteBufferOperation::readResolutionFromInputSocket()
{
	NodeOperation *inputOperation = this->getInputOperation(0);
	this->setWidth(inputOperation->getWidth());
	this->setHeight(inputOperation->getHeight());
}
