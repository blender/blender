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

#ifndef _COM_SetVectorOperation_h
#define _COM_SetVectorOperation_h
#include "COM_NodeOperation.h"


/**
 * this program converts an input colour to an output value.
 * it assumes we are in sRGB colour space.
 */
class SetVectorOperation : public NodeOperation {
private:
	float x;
	float y;
	float z;
	float w;

public:
	/**
	 * Default constructor
	 */
	SetVectorOperation();
	
	const float getX() { return this->x; }
	void setX(float value) { this->x = value; }
	const float getY() { return this->y; }
	void setY(float value) { this->y = value; }
	const float getZ() { return this->z; }
	void setZ(float value) { this->z = value; }
	const float getW() { return this->w; }
	void setW(float value) { this->w = value; }
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	const bool isSetOperation() const { return true; }

	void setVector(float vector[3]) {
		setX(vector[0]);
		setY(vector[1]);
		setZ(vector[2]);
	}
};
#endif
