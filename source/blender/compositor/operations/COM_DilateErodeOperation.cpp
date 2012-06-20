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

#include "COM_DilateErodeOperation.h"
#include "BLI_math.h"
#include "COM_OpenCLDevice.h"

// DilateErode Distance Threshold
DilateErodeThresholdOperation::DilateErodeThresholdOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->setComplex(true);
	this->inputProgram = NULL;
	this->inset = 0.0f;
	this->_switch = 0.5f;
	this->distance = 0.0f;
}
void DilateErodeThresholdOperation::initExecution()
{
	this->inputProgram = this->getInputSocketReader(0);
	if (this->distance < 0.0f) {
		this->scope = -this->distance + this->inset;
	}
	else {
		if (this->inset * 2 > this->distance) {
			this->scope = max(this->inset * 2 - this->distance, this->distance);
		}
		else {
			this->scope = distance;
		}
	}
	if (scope < 3) {
		scope = 3;
	}
}

void *DilateErodeThresholdOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = inputProgram->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void DilateErodeThresholdOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float inputValue[4];
	const float sw = this->_switch;
	const float distance = this->distance;
	float pixelvalue;
	const float rd = scope * scope;
	const float inset = this->inset;
	float mindist = rd * 2;

	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	rcti *rect = inputBuffer->getRect();
	const int minx = max(x - scope, rect->xmin);
	const int miny = max(y - scope, rect->ymin);
	const int maxx = min(x + scope, rect->xmax);
	const int maxy = min(y + scope, rect->ymax);
	const int bufferWidth = rect->xmax - rect->xmin;
	int offset;

	this->inputProgram->read(inputValue, x, y, inputBuffers, NULL);
	if (inputValue[0] > sw) {
		for (int yi = miny; yi < maxy; yi++) {
			const float dy = yi - y;
			offset = ((yi - rect->ymin) * bufferWidth + (minx - rect->xmin)) * 4;
			for (int xi = minx; xi < maxx; xi++) {
				if (buffer[offset] < sw) {
					const float dx = xi - x;
					const float dis = dx * dx + dy * dy;
					mindist = min(mindist, dis);
				}
				offset += 4;
			}
		}
		pixelvalue = -sqrtf(mindist);
	}
	else {
		for (int yi = miny; yi < maxy; yi++) {
			const float dy = yi - y;
			offset = ((yi - rect->ymin) * bufferWidth + (minx - rect->xmin)) * 4;
			for (int xi = minx; xi < maxx; xi++) {
				if (buffer[offset] > sw) {
					const float dx = xi - x;
					const float dis = dx * dx + dy * dy;
					mindist = min(mindist, dis);
				}
				offset += 4;

			}
		}
		pixelvalue = sqrtf(mindist);
	}

	if (distance > 0.0f) {
		const float delta = distance - pixelvalue;
		if (delta >= 0.0f) {
			if (delta >= inset) {
				color[0] = 1.0f;
			}
			else {
				color[0] = delta / inset;
			}
		}
		else {
			color[0] = 0.0f;
		}
	}
	else {
		const float delta = -distance + pixelvalue;
		if (delta < 0.0f) {
			if (delta < -inset) {
				color[0] = 1.0f;
			}
			else {
				color[0] = (-delta) / inset;
			}
		}
		else {
			color[0] = 0.0f;
		}
	}
}

void DilateErodeThresholdOperation::deinitExecution()
{
	this->inputProgram = NULL;
}

bool DilateErodeThresholdOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmax = input->xmax + scope;
	newInput.xmin = input->xmin - scope;
	newInput.ymax = input->ymax + scope;
	newInput.ymin = input->ymin - scope;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

// Dilate Distance
DilateDistanceOperation::DilateDistanceOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->setComplex(true);
	this->inputProgram = NULL;
	this->distance = 0.0f;
	this->setOpenCL(true);
}
void DilateDistanceOperation::initExecution()
{
	this->inputProgram = this->getInputSocketReader(0);
	this->scope = distance;
	if (scope < 3) {
		scope = 3;
	}
}

void *DilateDistanceOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = inputProgram->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void DilateDistanceOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	const float distance = this->distance;
	const float mindist = distance * distance;

	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	rcti *rect = inputBuffer->getRect();
	const int minx = max(x - scope, rect->xmin);
	const int miny = max(y - scope, rect->ymin);
	const int maxx = min(x + scope, rect->xmax);
	const int maxy = min(y + scope, rect->ymax);
	const int bufferWidth = rect->xmax - rect->xmin;
	int offset;
	
	float value = 0.0f;

	for (int yi = miny; yi < maxy; yi++) {
		const float dy = yi - y;
		offset = ((yi - rect->ymin) * bufferWidth + (minx - rect->xmin)) * 4;
		for (int xi = minx; xi < maxx; xi++) {
			const float dx = xi - x;
			const float dis = dx * dx + dy * dy;
			if (dis <= mindist) {
				value = max(buffer[offset], value);
			}
			offset += 4;
		}
	}
	color[0] = value;
}

void DilateDistanceOperation::deinitExecution()
{
	this->inputProgram = NULL;
}

bool DilateDistanceOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmax = input->xmax + scope;
	newInput.xmin = input->xmin - scope;
	newInput.ymax = input->ymax + scope;
	newInput.ymin = input->ymin - scope;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

static cl_kernel dilateKernel = 0;
void DilateDistanceOperation::executeOpenCL(OpenCLDevice* device,
                                            MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
                                            MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
                                            list<cl_kernel> *clKernelsToCleanUp)
{
	if (!dilateKernel) {
		dilateKernel = device->COM_clCreateKernel("dilateKernel", NULL);
	}
	cl_int distanceSquared = this->distance * this->distance;
	cl_int scope = this->scope;
	
	device->COM_clAttachMemoryBufferToKernelParameter(dilateKernel, 0,  2, clMemToCleanUp, inputMemoryBuffers, this->inputProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(dilateKernel, 1, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(dilateKernel, 3, outputMemoryBuffer);
	clSetKernelArg(dilateKernel, 4, sizeof(cl_int), &scope);
	clSetKernelArg(dilateKernel, 5, sizeof(cl_int), &distanceSquared);
	device->COM_clAttachSizeToKernelParameter(dilateKernel, 6, this);
	device->COM_clEnqueueRange(dilateKernel, outputMemoryBuffer, 7, this);
}

// Erode Distance
ErodeDistanceOperation::ErodeDistanceOperation() : DilateDistanceOperation() 
{
	/* pass */
}

void ErodeDistanceOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	const float distance = this->distance;
	const float mindist = distance * distance;

	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	rcti *rect = inputBuffer->getRect();
	const int minx = max(x - scope, rect->xmin);
	const int miny = max(y - scope, rect->ymin);
	const int maxx = min(x + scope, rect->xmax);
	const int maxy = min(y + scope, rect->ymax);
	const int bufferWidth = rect->xmax - rect->xmin;
	int offset;
	
	float value = 1.0f;

	for (int yi = miny; yi < maxy; yi++) {
		const float dy = yi - y;
		offset = ((yi - rect->ymin) * bufferWidth + (minx - rect->xmin)) * 4;
		for (int xi = minx; xi < maxx; xi++) {
			const float dx = xi - x;
			const float dis = dx * dx + dy * dy;
			if (dis <= mindist) {
				value = min(buffer[offset], value);
			}
			offset += 4;
		}
	}
	color[0] = value;
}

static cl_kernel erodeKernel = 0;
void ErodeDistanceOperation::executeOpenCL(OpenCLDevice* device,
                                           MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
                                           MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
                                           list<cl_kernel> *clKernelsToCleanUp)
{
	if (!erodeKernel) {
		erodeKernel = device->COM_clCreateKernel("erodeKernel", NULL);
	}
	cl_int distanceSquared = this->distance * this->distance;
	cl_int scope = this->scope;
	
	device->COM_clAttachMemoryBufferToKernelParameter(erodeKernel, 0,  2, clMemToCleanUp, inputMemoryBuffers, this->inputProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(erodeKernel, 1, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(erodeKernel, 3, outputMemoryBuffer);
	clSetKernelArg(erodeKernel, 4, sizeof(cl_int), &scope);
	clSetKernelArg(erodeKernel, 5, sizeof(cl_int), &distanceSquared);
	device->COM_clAttachSizeToKernelParameter(erodeKernel, 6, this);
	device->COM_clEnqueueRange(erodeKernel, outputMemoryBuffer, 7, this);
}

// Dilate step
DilateStepOperation::DilateStepOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->setComplex(true);
	this->inputProgram = NULL;
}
void DilateStepOperation::initExecution()
{
	this->inputProgram = this->getInputSocketReader(0);
	this->cached_buffer = NULL;
	this->initMutex();
}

void *DilateStepOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->cached_buffer != NULL) {
		return this->cached_buffer;
	}
	lockMutex();
	if (this->cached_buffer == NULL) {
		MemoryBuffer *buffer = (MemoryBuffer *)inputProgram->initializeTileData(NULL, memoryBuffers);
		float *rectf = buffer->convertToValueBuffer();
		int x, y, i;
		float *p;
		int bwidth = buffer->getWidth();
		int bheight = buffer->getHeight();
		for (i = 0; i < this->iterations; i++) {
			for (y = 0; y < bheight; y++) {
				for (x = 0; x < bwidth - 1; x++) {
					p = rectf + (bwidth * y + x);
					*p = MAX2(*p, *(p + 1));
				}
			}
		
			for (y = 0; y < bheight; y++) {
				for (x = bwidth - 1; x >= 1; x--) {
					p = rectf + (bwidth * y + x);
					*p = MAX2(*p, *(p - 1));
				}
			}
		
			for (x = 0; x < bwidth; x++) {
				for (y = 0; y < bheight - 1; y++) {
					p = rectf + (bwidth * y + x);
					*p = MAX2(*p, *(p + bwidth));
				}
			}
		
			for (x = 0; x < bwidth; x++) {
				for (y = bheight - 1; y >= 1; y--) {
					p = rectf + (bwidth * y + x);
					*p = MAX2(*p, *(p - bwidth));
				}
			}
		}
		this->cached_buffer = rectf;
	}
	unlockMutex();
	return this->cached_buffer;
}


void DilateStepOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	color[0] = this->cached_buffer[y * this->getWidth() + x];
}

void DilateStepOperation::deinitExecution()
{
	this->inputProgram = NULL;
	this->deinitMutex();
	if (this->cached_buffer) {
		delete cached_buffer;
		this->cached_buffer = NULL;
	}
}

bool DilateStepOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->cached_buffer) {
		return false;
	}
	else {
		rcti newInput;
	
		newInput.xmax = getWidth();
		newInput.xmin = 0;
		newInput.ymax = getHeight();
		newInput.ymin = 0;
	
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}

// Erode step
ErodeStepOperation::ErodeStepOperation() : DilateStepOperation()
{
	/* pass */
}

void *ErodeStepOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->cached_buffer != NULL) {
		return this->cached_buffer;
	}
	lockMutex();
	if (this->cached_buffer == NULL) {
		MemoryBuffer *buffer = (MemoryBuffer *)inputProgram->initializeTileData(NULL, memoryBuffers);
		float *rectf = buffer->convertToValueBuffer();
		int x, y, i;
		float *p;
		int bwidth = buffer->getWidth();
		int bheight = buffer->getHeight();
		for (i = 0; i < this->iterations; i++) {
			for (y = 0; y < bheight; y++) {
				for (x = 0; x < bwidth - 1; x++) {
					p = rectf + (bwidth * y + x);
					*p = MIN2(*p, *(p + 1));
				}
			}
		
			for (y = 0; y < bheight; y++) {
				for (x = bwidth - 1; x >= 1; x--) {
					p = rectf + (bwidth * y + x);
					*p = MIN2(*p, *(p - 1));
				}
			}
		
			for (x = 0; x < bwidth; x++) {
				for (y = 0; y < bheight - 1; y++) {
					p = rectf + (bwidth * y + x);
					*p = MIN2(*p, *(p + bwidth));
				}
			}
		
			for (x = 0; x < bwidth; x++) {
				for (y = bheight - 1; y >= 1; y--) {
					p = rectf + (bwidth * y + x);
					*p = MIN2(*p, *(p - bwidth));
				}
			}
		}
		this->cached_buffer = rectf;
	}
	unlockMutex();
	return this->cached_buffer;
}
