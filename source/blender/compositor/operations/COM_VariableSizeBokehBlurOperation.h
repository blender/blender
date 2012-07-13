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

#ifndef _COM_BokehVariableSizeBokehBlurOperation_h
#define _COM_VariableSizeBokehBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

//#define COM_DEFOCUS_SEARCH

class VariableSizeBokehBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	int m_maxBlur;
	float m_threshold;
	SocketReader *m_inputProgram;
	SocketReader *m_inputBokehProgram;
	SocketReader *m_inputSizeProgram;
#ifdef COM_DEFOCUS_SEARCH
	SocketReader *m_inputSearchProgram;
#endif

public:
	VariableSizeBokehBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	void *initializeTileData(rcti *rect);
	
	void deinitializeTileData(rcti *rect, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	
	void setMaxBlur(int maxRadius) { this->m_maxBlur = maxRadius; }

	void setThreshold(float threshold) { this->m_threshold = threshold; }

	void executeOpenCL(OpenCLDevice* device, MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp);
};

#ifdef COM_DEFOCUS_SEARCH
class InverseSearchRadiusOperation : public NodeOperation {
private:
	int m_maxBlur;
	SocketReader *m_inputRadius;
public:
	static const int DIVIDER = 4;
	
	InverseSearchRadiusOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	void* initializeTileData(rcti *rect);
	void deinitializeTileData(rcti *rect, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	void setMaxBlur(int maxRadius) { this->m_maxBlur = maxRadius; }
};
#endif
#endif
