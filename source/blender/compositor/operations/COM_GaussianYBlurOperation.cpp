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

#include "COM_GaussianYBlurOperation.h"
#include "COM_OpenCLDevice.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"

extern "C" {
#  include "RE_pipeline.h"
}

GaussianYBlurOperation::GaussianYBlurOperation() : BlurBaseOperation(COM_DT_COLOR)
{
	this->m_gausstab = NULL;
#ifdef __SSE2__
	this->m_gausstab_sse = NULL;
#endif
	this->m_filtersize = 0;
}

void *GaussianYBlurOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_sizeavailable) {
		updateGauss();
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL);
	unlockMutex();
	return buffer;
}

void GaussianYBlurOperation::initExecution()
{
	BlurBaseOperation::initExecution();

	initMutex();

	if (this->m_sizeavailable) {
		float rad = max_ff(m_size * m_data.sizey, 0.0f);
		m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

		this->m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
#ifdef __SSE2__
		this->m_gausstab_sse = BlurBaseOperation::convert_gausstab_sse(this->m_gausstab,
		                                                               rad,
		                                                               m_filtersize);
#endif
	}
}

void GaussianYBlurOperation::updateGauss()
{
	if (this->m_gausstab == NULL) {
		updateSize();
		float rad = max_ff(m_size * m_data.sizey, 0.0f);
		m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);

		this->m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
#ifdef __SSE2__
		this->m_gausstab_sse = BlurBaseOperation::convert_gausstab_sse(this->m_gausstab,
		                                                               rad,
		                                                               m_filtersize);
#endif
	}
}

void GaussianYBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	float color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float multiplier_accum = 0.0f;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	rcti &rect = *inputBuffer->getRect();
	int xmin = max_ii(x,                    rect.xmin);
	int ymin = max_ii(y - m_filtersize,     rect.ymin);
	int ymax = min_ii(y + m_filtersize + 1, rect.ymax);

	int index;
	int step = getStep();
	const int bufferIndexx = ((xmin - bufferstartx) * 4);

#ifdef __SSE2__
	__m128 accum_r = _mm_load_ps(color_accum);
	for (int ny = ymin; ny < ymax; ny += step) {
		index = (ny - y) + this->m_filtersize;
		int bufferindex = bufferIndexx + ((ny - bufferstarty) * 4 * bufferwidth);
		const float multiplier = this->m_gausstab[index];
		__m128 reg_a = _mm_load_ps(&buffer[bufferindex]);
		reg_a = _mm_mul_ps(reg_a, this->m_gausstab_sse[index]);
		accum_r = _mm_add_ps(accum_r, reg_a);
		multiplier_accum += multiplier;
	}
	_mm_store_ps(color_accum, accum_r);
#else
	for (int ny = ymin; ny < ymax; ny += step) {
		index = (ny - y) + this->m_filtersize;
		int bufferindex = bufferIndexx + ((ny - bufferstarty) * 4 * bufferwidth);
		const float multiplier = this->m_gausstab[index];
		madd_v4_v4fl(color_accum, &buffer[bufferindex], multiplier);
		multiplier_accum += multiplier;
	}
#endif
	mul_v4_v4fl(output, color_accum, 1.0f / multiplier_accum);
}

void GaussianYBlurOperation::executeOpenCL(OpenCLDevice *device,
                                           MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
                                           MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
                                           list<cl_kernel> *clKernelsToCleanUp)
{
	cl_kernel gaussianYBlurOperationKernel = device->COM_clCreateKernel("gaussianYBlurOperationKernel", NULL);
	cl_int filter_size = this->m_filtersize;

	cl_mem gausstab = clCreateBuffer(device->getContext(),
	                                 CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
	                                 sizeof(float) * (this->m_filtersize * 2 + 1),
	                                 this->m_gausstab,
	                                 NULL);

	device->COM_clAttachMemoryBufferToKernelParameter(gaussianYBlurOperationKernel, 0, 1, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(gaussianYBlurOperationKernel, 2, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(gaussianYBlurOperationKernel, 3, outputMemoryBuffer);
	clSetKernelArg(gaussianYBlurOperationKernel, 4, sizeof(cl_int), &filter_size);
	device->COM_clAttachSizeToKernelParameter(gaussianYBlurOperationKernel, 5, this);
	clSetKernelArg(gaussianYBlurOperationKernel, 6, sizeof(cl_mem), &gausstab);

	device->COM_clEnqueueRange(gaussianYBlurOperationKernel, outputMemoryBuffer, 7, this);

	clReleaseMemObject(gausstab);
}

void GaussianYBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();

	if (this->m_gausstab) {
		MEM_freeN(this->m_gausstab);
		this->m_gausstab = NULL;
	}
#ifdef __SSE2__
	if (this->m_gausstab_sse) {
		MEM_freeN(this->m_gausstab_sse);
		this->m_gausstab_sse = NULL;
	}
#endif

	deinitMutex();
}

bool GaussianYBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
	if (!m_sizeavailable) {
		rcti sizeInput;
		sizeInput.xmin = 0;
		sizeInput.ymin = 0;
		sizeInput.xmax = 5;
		sizeInput.ymax = 5;
		NodeOperation *operation = this->getInputOperation(1);
		if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
			return true;
		}
	}
	{
		if (this->m_sizeavailable && this->m_gausstab != NULL) {
			newInput.xmax = input->xmax;
			newInput.xmin = input->xmin;
			newInput.ymax = input->ymax + this->m_filtersize + 1;
			newInput.ymin = input->ymin - this->m_filtersize - 1;
		}
		else {
			newInput.xmax = this->getWidth();
			newInput.xmin = 0;
			newInput.ymax = this->getHeight();
			newInput.ymin = 0;
		}
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}
