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

extern "C" {
	#include "RE_pipeline.h"
}

BokehBlurOperation::BokehBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);

	this->size = 1.0f;

	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputBoundingBoxReader = NULL;
}

void *BokehBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void BokehBlurOperation::initExecution()
{
	this->inputProgram = getInputSocketReader(0);
	this->inputBokehProgram = getInputSocketReader(1);
	this->inputBoundingBoxReader = getInputSocketReader(2);

	int width = inputBokehProgram->getWidth();
	int height = inputBokehProgram->getHeight();

	float dimension;
	if (width<height) {
		dimension = width;
	}
	else {
		dimension = height;
	}
	this->bokehMidX = width/2.0f;
	this->bokehMidY = height/2.0f;
	this->bokehDimension = dimension/2.0f;
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void BokehBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float tempColor[4];
	float tempBoundingBox[4];
	float bokeh[4];

	inputBoundingBoxReader->read(tempBoundingBox, x, y, COM_PS_NEAREST, inputBuffers);
	if (tempBoundingBox[0] >0.0f) {
		tempColor[0] = 0;
		tempColor[1] = 0;
		tempColor[2] = 0;
		tempColor[3] = 0;
		float overallmultiplyerr = 0;
		float overallmultiplyerg = 0;
		float overallmultiplyerb = 0;
		float overallmultiplyera = 0;
		MemoryBuffer *inputBuffer = (MemoryBuffer*)data;
		float *buffer = inputBuffer->getBuffer();
		int bufferwidth = inputBuffer->getWidth();
		int bufferstartx = inputBuffer->getRect()->xmin;
		int bufferstarty = inputBuffer->getRect()->ymin;
		int pixelSize = this->size*this->getWidth()/100.0f;

		int miny = y - pixelSize;
		int maxy = y + pixelSize;
		int minx = x - pixelSize;
		int maxx = x + pixelSize;
		miny = max(miny, inputBuffer->getRect()->ymin);
		minx = max(minx, inputBuffer->getRect()->xmin);
		maxy = min(maxy, inputBuffer->getRect()->ymax);
		maxx = min(maxx, inputBuffer->getRect()->xmax);

		int step = getStep();
		int offsetadd = getOffsetAdd();

		float m = this->bokehDimension/pixelSize;
		for (int ny = miny ; ny < maxy ; ny +=step) {
			int bufferindex = ((minx - bufferstartx)*4)+((ny-bufferstarty)*4*bufferwidth);
			for (int nx = minx ; nx < maxx ; nx +=step) {
				float u = this->bokehMidX - (nx-x) *m;
				float v = this->bokehMidY - (ny-y) *m;
				inputBokehProgram->read(bokeh, u, v, COM_PS_NEAREST, inputBuffers);
				tempColor[0] += bokeh[0] * buffer[bufferindex];
				tempColor[1] += bokeh[1] * buffer[bufferindex+1];
				tempColor[2] += bokeh[2]* buffer[bufferindex+2];
				tempColor[3] += bokeh[3]* buffer[bufferindex+3];
				overallmultiplyerr += bokeh[0];
				overallmultiplyerg += bokeh[1];
				overallmultiplyerb += bokeh[2];
				overallmultiplyera += bokeh[3];
				bufferindex +=offsetadd;
			}
		}
		color[0] = tempColor[0] * (1.0f / overallmultiplyerr);
		color[1] = tempColor[1] * (1.0f / overallmultiplyerg);
		color[2] = tempColor[2] * (1.0f / overallmultiplyerb);
		color[3] = tempColor[3] * (1.0f / overallmultiplyera);
	}
	else {
		inputProgram->read(color, x, y, COM_PS_NEAREST, inputBuffers);
	}
}

void BokehBlurOperation::deinitExecution()
{
	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputBoundingBoxReader = NULL;
}

bool BokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti bokehInput;

	newInput.xmax = input->xmax + (size*this->getWidth()/100.0f);
	newInput.xmin = input->xmin - (size*this->getWidth()/100.0f);
	newInput.ymax = input->ymax + (size*this->getWidth()/100.0f);
	newInput.ymin = input->ymin - (size*this->getWidth()/100.0f);

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
	return false;
}

static cl_kernel kernel = 0;
void BokehBlurOperation::executeOpenCL(cl_context context, cl_program program, cl_command_queue queue, 
                                       MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, 
                                       MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, 
                                       list<cl_kernel> *clKernelsToCleanUp) 
{
	if (!kernel) {
		kernel = COM_clCreateKernel(program, "bokehBlurKernel", NULL);
	}
	cl_int radius = this->getWidth()*this->size/100.0f;
	cl_int step = this->getStep();
	
	COM_clAttachMemoryBufferToKernelParameter(context, kernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, this->inputBoundingBoxReader);
	COM_clAttachMemoryBufferToKernelParameter(context, kernel, 1,  4, clMemToCleanUp, inputMemoryBuffers, this->inputProgram);
	COM_clAttachMemoryBufferToKernelParameter(context, kernel, 2,  -1, clMemToCleanUp, inputMemoryBuffers, this->inputBokehProgram);
	COM_clAttachOutputMemoryBufferToKernelParameter(kernel, 3, clOutputBuffer);
	COM_clAttachMemoryBufferOffsetToKernelParameter(kernel, 5, outputMemoryBuffer);
	clSetKernelArg(kernel, 6, sizeof(cl_int), &radius);
	clSetKernelArg(kernel, 7, sizeof(cl_int), &step);
	COM_clAttachSizeToKernelParameter(kernel, 8);
	
	COM_clEnqueueRange(queue, kernel, outputMemoryBuffer, 9);	
}
