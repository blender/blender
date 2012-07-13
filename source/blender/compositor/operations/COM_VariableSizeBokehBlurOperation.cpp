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
#ifdef COM_DEFOCUS_SEARCH
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // inverse search radius optimization structure.
#endif
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);

	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
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
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = getInputSocketReader(3);
#endif
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void *VariableSizeBokehBlurOperation::initializeTileData(rcti *rect)
{
	MemoryBuffer** result = new MemoryBuffer*[3];
	result[0] = (MemoryBuffer*)this->m_inputProgram->initializeTileData(rect);
	result[1] = (MemoryBuffer*)this->m_inputBokehProgram->initializeTileData(rect);
	result[2] = (MemoryBuffer*)this->m_inputSizeProgram->initializeTileData(rect);
	return result;
}

void VariableSizeBokehBlurOperation::deinitializeTileData(rcti *rect, void *data)
{
	MemoryBuffer** result = (MemoryBuffer**)data;
	delete[] result;
}

void VariableSizeBokehBlurOperation::executePixel(float *color, int x, int y, void *data)
{
	MemoryBuffer** buffers = (MemoryBuffer**)data;
	MemoryBuffer* inputProgramBuffer = buffers[0];
	MemoryBuffer* inputBokehBuffer = buffers[1];
	MemoryBuffer* inputSizeBuffer = buffers[2];
	float* inputSizeFloatBuffer = inputSizeBuffer->getBuffer();
	float* inputProgramFloatBuffer = inputProgramBuffer->getBuffer();
	float readColor[4];
	float bokeh[4];
	float tempSize[4];
	float multiplier_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float color_accum[4]      = {0.0f, 0.0f, 0.0f, 0.0f};

#ifdef COM_DEFOCUS_SEARCH
	float search[4];
	this->m_inputSearchProgram->read(search, x/InverseSearchRadiusOperation::DIVIDER, y / InverseSearchRadiusOperation::DIVIDER, NULL);
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
		inputSizeBuffer->readNoCheck(tempSize, x, y);
		inputProgramBuffer->readNoCheck(readColor, x, y);

		add_v4_v4(color_accum, readColor);
		add_v4_fl(multiplier_accum, 1.0f);
		float sizeCenter = tempSize[0];
		
		const int addXStep = QualityStepHelper::getStep()*COM_NUMBER_OF_CHANNELS;
		
		if (sizeCenter > this->m_threshold) {
			for (int ny = miny; ny < maxy; ny += QualityStepHelper::getStep()) {
				float dy = ny - y;
				int offsetNy = ny * inputSizeBuffer->getWidth() * COM_NUMBER_OF_CHANNELS;
				int offsetNxNy = offsetNy + (minx*COM_NUMBER_OF_CHANNELS);
				for (int nx = minx; nx < maxx; nx += QualityStepHelper::getStep()) {
					if (nx != x || ny != y) 
					{
						float size = inputSizeFloatBuffer[offsetNxNy];
						if (size > this->m_threshold) {
							float fsize = fabsf(size);
							float dx = nx - x;
							if (fsize > fabsf(dx) && fsize > fabsf(dy)) {
								float u = (256.0f + (dx/size) * 255.0f);
								float v = (256.0f + (dy/size) * 255.0f);
								inputBokehBuffer->readNoCheck(bokeh, u, v);
								madd_v4_v4v4(color_accum, bokeh, &inputProgramFloatBuffer[offsetNxNy]);
								add_v4_v4(multiplier_accum, bokeh);
							}
						}
					}
					offsetNxNy += addXStep;
				}
			}
		}

		color[0] = color_accum[0] / multiplier_accum[0];
		color[1] = color_accum[1] / multiplier_accum[1];
		color[2] = color_accum[2] / multiplier_accum[2];
		color[3] = color_accum[3] / multiplier_accum[3];
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
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 2,  4, clMemToCleanUp, inputMemoryBuffers, this->m_inputSizeProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(defocusKernel, 3, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(defocusKernel, 5, outputMemoryBuffer);
	clSetKernelArg(defocusKernel, 6, sizeof(cl_int), &step);
	clSetKernelArg(defocusKernel, 7, sizeof(cl_int), &maxBlur);
	clSetKernelArg(defocusKernel, 8, sizeof(cl_float), &threshold);
	device->COM_clAttachSizeToKernelParameter(defocusKernel, 9, this);
	
	device->COM_clEnqueueRange(defocusKernel, outputMemoryBuffer, 10, this);
}

void VariableSizeBokehBlurOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
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
#ifdef COM_DEFOCUS_SEARCH
	rcti searchInput;
	searchInput.xmax = (input->xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.xmin = (input->xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
	searchInput.ymax = (input->ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.ymin = (input->ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
	operation = getInputOperation(3);
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
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputRadius = NULL;
}

void InverseSearchRadiusOperation::initExecution() 
{
	this->m_inputRadius = this->getInputSocketReader(0);
}

void* InverseSearchRadiusOperation::initializeTileData(rcti *rect) 
{
	MemoryBuffer * data = new MemoryBuffer(NULL, rect);
	float* buffer = data->getBuffer();
	int x, y;
	int width = this->m_inputRadius->getWidth();
	int height = this->m_inputRadius->getHeight();
	float temp[4];
	int offset = 0;
	for (y = rect->ymin; y < rect->ymax ; y++) {
		for (x = rect->xmin; x < rect->xmax ; x++) {
			int rx = x * DIVIDER;
			int ry = y * DIVIDER;
			buffer[offset] = MAX2(rx - m_maxBlur, 0);
			buffer[offset+1] = MAX2(ry- m_maxBlur, 0);
			buffer[offset+2] = MIN2(rx+DIVIDER + m_maxBlur, width);
			buffer[offset+3] = MIN2(ry+DIVIDER + m_maxBlur, height);
			offset += 4;
		}
	}
//	for (x = rect->xmin; x < rect->xmax ; x++) {
//		for (y = rect->ymin; y < rect->ymax ; y++) {
//			int rx = x * DIVIDER;
//			int ry = y * DIVIDER;
//			float radius = 0.0f;
//			float maxx = x;
//			float maxy = y;
	
//			for (int x2 = 0 ; x2 < DIVIDER ; x2 ++) {
//				for (int y2 = 0 ; y2 < DIVIDER ; y2 ++) {
//					this->m_inputRadius->read(temp, rx+x2, ry+y2, COM_PS_NEAREST);
//					if (radius < temp[0]) {
//						radius = temp[0];
//						maxx = x2;
//						maxy = y2;
//					}
//				}
//			}
//			int impactRadius = ceil(radius / DIVIDER);
//			for (int x2 = x - impactRadius ; x2 < x + impactRadius ; x2 ++) {
//				for (int y2 = y - impactRadius ; y2 < y + impactRadius ; y2 ++) {
//					data->read(temp, x2, y2);
//					temp[0] = MIN2(temp[0], maxx);
//					temp[1] = MIN2(temp[1], maxy);
//					temp[2] = MAX2(temp[2], maxx);
//					temp[3] = MAX2(temp[3], maxy);
//					data->writePixel(x2, y2, temp);
//				}
//			}
//		}
//	}
	return data;
}

void InverseSearchRadiusOperation::executePixel(float *color, int x, int y, void *data) 
{
	MemoryBuffer *buffer = (MemoryBuffer*)data;
	buffer->readNoCheck(color, x, y);
}

void InverseSearchRadiusOperation::deinitializeTileData(rcti *rect, void *data) 
{
	if (data) {
		MemoryBuffer* mb = (MemoryBuffer*)data;
		delete mb;
	}
}

void InverseSearchRadiusOperation::deinitExecution() 
{
	this->m_inputRadius = NULL;
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
	newRect.ymin = input->ymin*DIVIDER - m_maxBlur;
	newRect.ymax = input->ymax*DIVIDER + m_maxBlur;
	newRect.xmin = input->xmin*DIVIDER - m_maxBlur;
	newRect.xmax = input->xmax*DIVIDER + m_maxBlur;
	return NodeOperation::determineDependingAreaOfInterest(&newRect, readOperation, output);
}
#endif
