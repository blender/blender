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

#ifndef _COM_FastGaussianBlurOperation_h
#define _COM_FastGaussianBlurOperation_h

#include "COM_BlurBaseOperation.h"
#include "DNA_node_types.h"

class FastGaussianBlurOperation : public BlurBaseOperation {
private:
	float m_sx;
	float m_sy;
	MemoryBuffer *m_iirgaus;
public:
	FastGaussianBlurOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	static void IIR_gauss(MemoryBuffer *src, float sigma, unsigned int channel, unsigned int xy);
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void deinitExecution();
	void initExecution();
};

class FastGaussianBlurValueOperation : public NodeOperation {
private:
	float m_sigma;
	MemoryBuffer *m_iirgaus;
	SocketReader *m_inputprogram;
public:
	FastGaussianBlurValueOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void deinitExecution();
	void initExecution();
	void setSigma(float sigma) { this->m_sigma = sigma; }
};

#endif

