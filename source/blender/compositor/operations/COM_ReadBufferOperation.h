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

#ifndef _COM_ReadBufferOperation_h
#define _COM_ReadBufferOperation_h

#include "COM_NodeOperation.h"
#include "COM_MemoryProxy.h"

class ReadBufferOperation : public NodeOperation {
private:
	MemoryProxy *m_memoryProxy;
	unsigned int m_offset;
public:
	ReadBufferOperation();
	int isBufferOperation() { return true; }
	void setMemoryProxy(MemoryProxy *memoryProxy) { this->m_memoryProxy = memoryProxy; }
	MemoryProxy *getMemoryProxy() { return this->m_memoryProxy; }
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
	void executePixel(float *color, float x, float y, float dx, float dy, MemoryBuffer * inputBuffers[]);
	const bool isReadBufferOperation() const { return true; }
	void setOffset(unsigned int offset) { this->m_offset = offset; }
	unsigned int getOffset() { return this->m_offset; }
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	MemoryBuffer *getInputMemoryBuffer(MemoryBuffer **memoryBuffers) { return memoryBuffers[this->m_offset]; }
	void readResolutionFromWriteBuffer();
};

#endif
