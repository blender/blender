/*
 * Copyright 2012, Blender Foundation.
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
 *		Dalai Felinto
 */

#ifndef _COM_DisplaceSimpleOperation_h
#define _COM_DisplaceSimpleOperation_h
#include "COM_NodeOperation.h"


class DisplaceSimpleOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader* inputColorProgram;
	SocketReader* inputVectorProgram;
	SocketReader* inputScaleXProgram;
	SocketReader* inputScaleYProgram;

	float width_x4;
	float height_x4;

public:
	DisplaceSimpleOperation();

	/**
	* we need a full buffer for the image
	*/	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	/**
	 * the inner loop of this program
	 */
	void executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
};
#endif
