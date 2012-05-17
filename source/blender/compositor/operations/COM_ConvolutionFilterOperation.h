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

#ifndef _COM_ConvolutionFilterOperation_h_
#define _COM_ConvolutionFilterOperation_h_

#include "COM_NodeOperation.h"

class ConvolutionFilterOperation: public NodeOperation {
private:
	int filterWidth;
	int filterHeight;

protected:
	SocketReader *inputOperation;
	SocketReader *inputValueOperation;
	float *filter;

public:
	ConvolutionFilterOperation();
	void set3x3Filter(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color,int x, int y, MemoryBuffer *inputBuffers[], void *data);
	
	void initExecution();
	void deinitExecution();
};

#endif
