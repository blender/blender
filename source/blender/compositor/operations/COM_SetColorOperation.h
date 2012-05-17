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

#ifndef _COM_SetColorOperation_h
#define _COM_SetColorOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class SetColorOperation : public NodeOperation {
private:
	float channel1;
	float channel2;
	float channel3;
	float channel4;

public:
	/**
	  * Default constructor
	  */
	SetColorOperation();

	const float getChannel1() {return this->channel1;}
	void setChannel1(float value) {this->channel1 = value;}
	const float getChannel2() {return this->channel2;}
	void setChannel2(float value) {this->channel2 = value;}
	const float getChannel3() {return this->channel3;}
	void setChannel3(float value) {this->channel3 = value;}
	const float getChannel4() {return this->channel4;}
	void setChannel4(float value) {this->channel4 = value;}
	void setChannels(float value[4]) {this->channel1 = value[0];this->channel2 = value[1];this->channel3 = value[2];this->channel4 = value[3];}

	/**
	  * the inner loop of this program
	  */
	void executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	const bool isSetOperation() const {return true;}

};
#endif
