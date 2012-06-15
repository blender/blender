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

#ifndef _COM_ChangeHSVOperation_h
#define _COM_ChangeHSVOperation_h
#include "COM_MixBaseOperation.h"


/**
 * this program converts an input colour to an output value.
 * it assumes we are in sRGB colour space.
 */
class ChangeHSVOperation : public NodeOperation {
private:
	SocketReader *inputOperation;

	float hue;
	float saturation;
	float value;

public:
	/**
	 * Default constructor
	 */
	ChangeHSVOperation();
	
	void initExecution();
	void deinitExecution();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);

	void setHue(float hue) { this->hue = hue; }
	void setSaturation(float saturation) { this->saturation = saturation; }
	void setValue(float value) { this->value = value; }

};
#endif
