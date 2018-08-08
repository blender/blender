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

#ifndef __COM_GAUSSIANYBLUROPERATION_H__
#define __COM_GAUSSIANYBLUROPERATION_H__
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"

class GaussianYBlurOperation : public BlurBaseOperation {
private:
	float *m_gausstab;
#ifdef __SSE2__
	__m128 *m_gausstab_sse;
#endif
	int m_filtersize;
	void updateGauss();
public:
	GaussianYBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	void executeOpenCL(OpenCLDevice *device,
	                   MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
	                   MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
	                   list<cl_kernel> *clKernelsToCleanUp);

	/**
	 * @brief initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void *initializeTileData(rcti *rect);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void checkOpenCL() {
		this->setOpenCL(m_data.sizex >= 128);
	}
};
#endif
