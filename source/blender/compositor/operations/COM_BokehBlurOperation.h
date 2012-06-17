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

#ifndef _COM_BokehBokehBlurOperation_h
#define _COM_BokehBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class BokehBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	SocketReader *inputProgram;
	SocketReader *inputBokehProgram;
	SocketReader *inputBoundingBoxReader;
	float size;
	float bokehMidX;
	float bokehMidY;
	float bokehDimension;
public:
	BokehBlurOperation();

	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setSize(float size) { this->size = size; }
	
	void executeOpenCL(cl_context context, cl_program program, cl_command_queue queue, MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp);
};
#endif
