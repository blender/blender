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
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */


#ifndef _COM_KeyingOperation_h
#define _COM_KeyingOperation_h

#include <string.h>

#include "COM_NodeOperation.h"

#include "BLI_listbase.h"

/**
  * Class with implementation of keying node
  */
class KeyingOperation : public NodeOperation {
protected:
	SocketReader *pixelReader;
	SocketReader *screenReader;
	SocketReader *garbageReader;

	float screenBalance;

public:
	KeyingOperation();

	void initExecution();
	void deinitExecution();

	void setScreenBalance(float value) {this->screenBalance = value;}

	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

#endif
