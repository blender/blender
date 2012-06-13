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

#ifndef _COM_WriteBufferOperation_h_
#define _COM_WriteBufferOperation_h_

#include "COM_NodeOperation.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"
/**
  * @brief Operation to write to a tile
  * @ingroup Operation
  */
class WriteBufferOperation: public NodeOperation {
	MemoryProxy *memoryProxy;
	NodeOperation *input;
public:
	WriteBufferOperation();
	~WriteBufferOperation();
	int isBufferOperation() {return true;}
	MemoryProxy *getMemoryProxy() {return this->memoryProxy;}
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
	const bool isWriteBufferOperation() const {return true;}
	
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers);
	void initExecution();
	void deinitExecution();
	void executeOpenCLRegion(cl_context context, cl_program program, cl_command_queue queue, rcti *rect, unsigned int chunkNumber, MemoryBuffer** memoryBuffers, MemoryBuffer* outputBuffer);
	void readResolutionFromInputSocket();

};
#endif
