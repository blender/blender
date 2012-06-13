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

#ifndef _COM_TranslateOperation_h_
#define _COM_TranslateOperation_h_

#include "COM_NodeOperation.h"

class TranslateOperation : public NodeOperation {
private:
	SocketReader *inputOperation;
	SocketReader *inputXOperation;
	SocketReader *inputYOperation;
	float deltaX;
	float deltaY;
	float isDeltaSet;
public:
	TranslateOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void initExecution();
	void deinitExecution();

	float getDeltaX() { return this->deltaX; }
	float getDeltaY() { return this->deltaY; }
	
	inline void ensureDelta() {
		if (!isDeltaSet) {
			float tempDelta[4];
			this->inputXOperation->read(tempDelta, 0, 0, COM_PS_NEAREST, NULL);
			this->deltaX = tempDelta[0];
			this->inputYOperation->read(tempDelta, 0, 0, COM_PS_NEAREST, NULL);
			this->deltaY = tempDelta[0];
			this->isDeltaSet = true;
		}
	}
};

#endif
