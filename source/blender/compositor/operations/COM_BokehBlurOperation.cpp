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

#include "COM_BokehBlurOperation.h"
#include "BLI_math.h"
#include "COM_OpenCLDevice.h"

extern "C" {
	#include "RE_pipeline.h"
}

BokehBlurOperation::BokehBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);

	this->m_size = 1.0f;
	this->m_sizeavailable = false;
	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputBoundingBoxReader = NULL;
}

void *BokehBlurOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_sizeavailable) {
		updateSize();
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL);
	unlockMutex();
	return buffer;
}

void BokehBlurOperation::initExecution()
{
	initMutex();
	this->m_inputProgram = getInputSocketReader(0);
	this->m_inputBokehProgram = getInputSocketReader(1);
	this->m_inputBoundingBoxReader = getInputSocketReader(2);

	int width = this->m_inputBokehProgram->getWidth();
	int height = this->m_inputBokehProgram->getHeight();

	float dimension;
	if (width < height) {
		dimension = width;
	}
	else {
		dimension = height;
	}
	this->m_bokehMidX = width / 2.0f;
	this->m_bokehMidY = height / 2.0f;
	this->m_bokehDimension = dimension / 2.0f;
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void BokehBlurOperation::executePixel(float *color, int x, int y, void *data)
{
	float color_accum[4];
	float tempBoundingBox[4];
	float bokeh[4];

	this->m_inputBoundingBoxReader->read(tempBoundingBox, x, y, COM_PS_NEAREST);
	if (tempBoundingBox[0] > 0.0f) {
		float multiplier_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
		float *buffer = inputBuffer->getBuffer();
		int bufferwidth = inputBuffer->getWidth();
		int bufferstartx = inputBuffer->getRect()->xmin;
		int bufferstarty = inputBuffer->getRect()->ymin;
		int pixelSize = this->m_size * this->getWidth() / 100.0f;
		if (pixelSize==0){
			this->m_inputProgram->read(color, x, y, COM_PS_NEAREST);
			return;
		}
		int miny = y - pixelSize;
		int maxy = y + pixelSize;
		int minx = x - pixelSize;
		int maxx = x + pixelSize;
		miny = max(miny, inputBuffer->getRect()->ymin);
		minx = max(minx, inputBuffer->getRect()->xmin);
		maxy = min(maxy, inputBuffer->getRect()->ymax);
		maxx = min(maxx, inputBuffer->getRect()->xmax);

		zero_v4(color_accum);

		int step = getStep();
		int offsetadd = getOffsetAdd();

		float m = this->m_bokehDimension / pixelSize;
		for (int ny = miny; ny < maxy; ny += step) {
			int bufferindex = ((minx - bufferstartx) * 4) + ((ny - bufferstarty) * 4 * bufferwidth);
			for (int nx = minx; nx < maxx; nx += step) {
				float u = this->m_bokehMidX - (nx - x) * m;
				float v = this->m_bokehMidY - (ny - y) * m;
				this->m_inputBokehProgram->read(bokeh, u, v, COM_PS_NEAREST);
				madd_v4_v4v4(color_accum, bokeh, &buffer[bufferindex]);
				add_v4_v4(multiplier_accum, bokeh);
				bufferindex += offsetadd;
			}
		}
		color[0] = color_accum[0] * (1.0f / multiplier_accum[0]);
		color[1] = color_accum[1] * (1.0f / multiplier_accum[1]);
		color[2] = color_accum[2] * (1.0f / multiplier_accum[2]);
		color[3] = color_accum[3] * (1.0f / multiplier_accum[3]);
	}
	else {
		this->m_inputProgram->read(color, x, y, COM_PS_NEAREST);
	}
}

void BokehBlurOperation::deinitExecution()
{
	deinitMutex();
	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputBoundingBoxReader = NULL;
}

bool BokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti bokehInput;

	if (this->m_sizeavailable) {
		newInput.xmax = input->xmax + (this->m_size * this->getWidth() / 100.0f);
		newInput.xmin = input->xmin - (this->m_size * this->getWidth() / 100.0f);
		newInput.ymax = input->ymax + (this->m_size * this->getWidth() / 100.0f);
		newInput.ymin = input->ymin - (this->m_size * this->getWidth() / 100.0f);
	} else {
		newInput.xmax = input->xmax + (10.0f * this->getWidth() / 100.0f);
		newInput.xmin = input->xmin - (10.0f * this->getWidth() / 100.0f);
		newInput.ymax = input->ymax + (10.0f * this->getWidth() / 100.0f);
		newInput.ymin = input->ymin - (10.0f * this->getWidth() / 100.0f);
	}

	NodeOperation *operation = getInputOperation(1);
	bokehInput.xmax = operation->getWidth();
	bokehInput.xmin = 0;
	bokehInput.ymax = operation->getHeight();
	bokehInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}
	if (!this->m_sizeavailable) {
		rcti sizeInput;
		sizeInput.xmin = 0;
		sizeInput.ymin = 0;
		sizeInput.xmax = 5;
		sizeInput.ymax = 5;
		operation = getInputOperation(3);
		if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output) ) {
			return true;
		}
	}
	return false;
}

void BokehBlurOperation::executeOpenCL(OpenCLDevice* device,
                                       MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, 
                                       MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, 
                                       list<cl_kernel> *clKernelsToCleanUp) 
{
	cl_kernel kernel = device->COM_clCreateKernel("bokehBlurKernel", NULL);
	if (!this->m_sizeavailable) {
		updateSize();
	}
	cl_int radius = this->getWidth() * this->m_size / 100.0f;
	cl_int step = this->getStep();
	
	device->COM_clAttachMemoryBufferToKernelParameter(kernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputBoundingBoxReader);
	device->COM_clAttachMemoryBufferToKernelParameter(kernel, 1,  4, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(kernel, 2,  -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputBokehProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(kernel, 3, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(kernel, 5, outputMemoryBuffer);
	clSetKernelArg(kernel, 6, sizeof(cl_int), &radius);
	clSetKernelArg(kernel, 7, sizeof(cl_int), &step);
	device->COM_clAttachSizeToKernelParameter(kernel, 8, this);
	
	device->COM_clEnqueueRange(kernel, outputMemoryBuffer, 9, this);
}

void BokehBlurOperation::updateSize()
{
	if (!this->m_sizeavailable) {
		float result[4];
		this->getInputSocketReader(3)->read(result, 0, 0, COM_PS_NEAREST);
		this->m_size = result[0];
		CLAMP(this->m_size, 0.0f, 10.0f);
		this->m_sizeavailable = true;
	}
}
