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

#include "COM_VariableSizeBokehBlurOperation.h"
#include "BLI_math.h"
#include "COM_OpenCLDevice.h"

extern "C" {
	#include "RE_pipeline.h"
}

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // do not resize the bokeh image.
	this->addInputSocket(COM_DT_VALUE); // radius
	this->addInputSocket(COM_DT_VALUE); // depth
#ifdef COM_DEFOCUS_SEARCH
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // inverse search radius optimization structure.
#endif
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);

	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
	this->m_inputDepthProgram = NULL;
	this->m_maxBlur = 32.0f;
	this->m_threshold = 1.0f;
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = NULL;
#endif
}


void VariableSizeBokehBlurOperation::initExecution()
{
	this->m_inputProgram = getInputSocketReader(0);
	this->m_inputBokehProgram = getInputSocketReader(1);
	this->m_inputSizeProgram = getInputSocketReader(2);
	this->m_inputDepthProgram = getInputSocketReader(3);
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = getInputSocketReader(4);
#endif
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void VariableSizeBokehBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float readColor[4];
	float bokeh[4];
	float tempSize[4];
	float tempDepth[4];
	float multiplier_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float color_accum[4]      = {0.0f, 0.0f, 0.0f, 0.0f};

#ifdef COM_DEFOCUS_SEARCH
	float search[4];
	this->inputSearchProgram->read(search, x/InverseSearchRadiusOperation::DIVIDER, y / InverseSearchRadiusOperation::DIVIDER, inputBuffers, NULL);
	int minx = search[0];
	int miny = search[1];
	int maxx = search[2];
	int maxy = search[3];
#else
	int minx = MAX2(x - this->m_maxBlur, 0.0f);
	int miny = MAX2(y - this->m_maxBlur, 0.0f);
	int maxx = MIN2(x + this->m_maxBlur, m_width);
	int maxy = MIN2(y + this->m_maxBlur, m_height);
#endif
	{
		this->m_inputSizeProgram->read(tempSize, x, y, COM_PS_NEAREST, inputBuffers);
		this->m_inputDepthProgram->read(tempDepth, x, y, COM_PS_NEAREST, inputBuffers);
		this->m_inputProgram->read(readColor, x, y, COM_PS_NEAREST, inputBuffers);
		add_v4_v4(color_accum, readColor);
		add_v4_fl(multiplier_accum, 1.0f);
		float sizeCenter = tempSize[0];
		float centerDepth = tempDepth[0] + this->m_threshold;
		
		for (int ny = miny; ny < maxy; ny += QualityStepHelper::getStep()) {
			for (int nx = minx; nx < maxx; nx += QualityStepHelper::getStep()) {
				if (nx >= 0 && nx < this->getWidth() && ny >= 0 && ny < getHeight()) {
					this->m_inputDepthProgram->read(tempDepth, nx, ny, COM_PS_NEAREST, inputBuffers);
					if (tempDepth[0] < centerDepth) {
						this->m_inputSizeProgram->read(tempSize, nx, ny, COM_PS_NEAREST, inputBuffers);
						float size = tempSize[0];
						if ((sizeCenter > this->m_threshold && size > this->m_threshold) || size <= this->m_threshold) {
							float dx = nx - x;
							float dy = ny - y;
							if (nx == x && ny == y) {
							}
							else if (size >= fabsf(dx) && size >= fabsf(dy)) {
								float u = 256 + dx * 256 / size;
								float v = 256 + dy * 256 / size;
								this->m_inputBokehProgram->read(bokeh, u, v, COM_PS_NEAREST, inputBuffers);
								this->m_inputProgram->read(readColor, nx, ny, COM_PS_NEAREST, inputBuffers);
								madd_v4_v4v4(color_accum, bokeh, readColor);
								add_v4_v4(multiplier_accum, bokeh);
							}
						}
					}
				}
			}
		}

		color[0] = color_accum[0] * (1.0f / multiplier_accum[0]);
		color[1] = color_accum[1] * (1.0f / multiplier_accum[1]);
		color[2] = color_accum[2] * (1.0f / multiplier_accum[2]);
		color[3] = color_accum[3] * (1.0f / multiplier_accum[3]);
	}

}

void VariableSizeBokehBlurOperation::executeOpenCL(OpenCLDevice* device,
                                       MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, 
                                       MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, 
                                       list<cl_kernel> *clKernelsToCleanUp) 
{
	cl_kernel defocusKernel = device->COM_clCreateKernel("defocusKernel", NULL);

	cl_int step = this->getStep();
	cl_int maxBlur = this->m_maxBlur;
	cl_float threshold = this->m_threshold;
	
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 1,  -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputBokehProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 2,  5, clMemToCleanUp, inputMemoryBuffers, this->m_inputDepthProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 3,  -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputSizeProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(defocusKernel, 4, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(defocusKernel, 6, outputMemoryBuffer);
	clSetKernelArg(defocusKernel, 7, sizeof(cl_int), &step);
	clSetKernelArg(defocusKernel, 8, sizeof(cl_int), &maxBlur);
	clSetKernelArg(defocusKernel, 9, sizeof(cl_float), &threshold);
	device->COM_clAttachSizeToKernelParameter(defocusKernel, 10, this);
	
	device->COM_clEnqueueRange(defocusKernel, outputMemoryBuffer, 11, this);
}

void VariableSizeBokehBlurOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
	this->m_inputDepthProgram = NULL;
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = NULL;
#endif
}

bool VariableSizeBokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti bokehInput;

	newInput.xmax = input->xmax + this->m_maxBlur + 2;
	newInput.xmin = input->xmin - this->m_maxBlur + 2;
	newInput.ymax = input->ymax + this->m_maxBlur - 2;
	newInput.ymin = input->ymin - this->m_maxBlur - 2;
	bokehInput.xmax = 512;
	bokehInput.xmin = 0;
	bokehInput.ymax = 512;
	bokehInput.ymin = 0;
	

	NodeOperation *operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(3);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
#ifdef COM_DEFOCUS_SEARCH
	rcti searchInput;
	searchInput.xmax = (input->xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.xmin = (input->xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
	searchInput.ymax = (input->ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.ymin = (input->ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
	operation = getInputOperation(4);
	if (operation->determineDependingAreaOfInterest(&searchInput, readOperation, output) ) {
		return true;
	}
#endif
	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	return false;
}

#ifdef COM_DEFOCUS_SEARCH
// InverseSearchRadiusOperation
InverseSearchRadiusOperation::InverseSearchRadiusOperation() : NodeOperation() 
{
	this->addInputSocket(COM_DT_VALUE, COM_SC_NO_RESIZE); // radius
	this->addInputSocket(COM_DT_VALUE, COM_SC_NO_RESIZE); // depth
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->inputRadius = NULL;
	this->inputDepth = NULL;
}

void InverseSearchRadiusOperation::initExecution() 
{
	this->inputRadius = this->getInputSocketReader(0);
	this->inputDepth = this->getInputSocketReader(1);
}

void* InverseSearchRadiusOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) 
{
	MemoryBuffer * data = new MemoryBuffer(NULL, rect);
	int x, y;
	float width = this->inputRadius->getWidth();
	float height = this->inputRadius->getHeight();
	        
	for (x = rect->xmin; x < rect->xmax ; x++) {
		for (y = rect->ymin; y < rect->ymax ; y++) {
			float[4] temp;
			int rx = x * DIVIDER;
			int ry = y * DIVIDER;
			this->inputRadius->read(temp, rx, ry, memoryBuffers, NULL);
			float centerRadius = temp[0];
			this->inputDepth->read(temp, rx, ry, memoryBuffers, NULL);
			float centerDepth = temp[0];
			t[0] = MAX2(rx - this->maxBlur, 0.0f);
			t[1] = MAX2(ry - this->maxBlur, 0.0f);
			t[2] = MIN2(rx + this->maxBlur, width);
			t[3] = MIN2(ry + this->maxBlur, height);
			int minx = t[0];
			int miny = t[1];
			int maxx = t[2];
			int maxy = t[3];
			int sminx = rx;
			int smaxx = rx;
			int sminy = ry;
			int smaxy = ry;
			for (int nx = minx ; nx < maxx ; nx ++) {
				for (int ny = miny ; ny < maxy ; ny ++) {
					this->inputRadius->read(temp, nx, ny, memoryBuffers, NULL);
					if (nx < rx && temp[0])
					
				}
			}
			float t[4];
			data->writePixel(x, y, t);
		}
	}
	return data;
}

void InverseSearchRadiusOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data) 
{
	MemoryBuffer *buffer = (MemoryBuffer*)data;
	buffer->read(color, x, y);
}

void InverseSearchRadiusOperation::deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data) 
{
	if (data) {
		MemoryBuffer* mb = (MemoryBuffer*)data;
		delete mb;
	}
}

void InverseSearchRadiusOperation::deinitExecution() 
{
	this->inputRadius = NULL;
	this->inputDepth = NULL;
}

void InverseSearchRadiusOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	NodeOperation::determineResolution(resolution, preferredResolution);
	resolution[0] = resolution[0] / DIVIDER;
	resolution[1] = resolution[1] / DIVIDER;
}

bool InverseSearchRadiusOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newRect;
	newRect.ymin = input->ymin*DIVIDER;
	newRect.ymax = input->ymax*DIVIDER;
	newRect.xmin = input->xmin*DIVIDER;
	newRect.xmax = input->xmax*DIVIDER;
	return NodeOperation::determineDependingAreaOfInterest(&newRect, readOperation, output);
}
#endif
