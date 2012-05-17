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

#include "COM_MixDarkenOperation.h"

MixDarkenOperation::MixDarkenOperation(): MixBaseOperation() {
}

void MixDarkenOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputColor1[4];
	float inputColor2[4];
	float value;
	
	inputValueOperation->read(&value, x, y, sampler, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, sampler, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, sampler, inputBuffers);
	
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f-value;
	float tmp;
	tmp=inputColor2[0]+((1.0f-inputColor2[0])*valuem);
	if (tmp < inputColor1[0]) outputValue[0]= tmp;
	else outputValue[0] = inputColor1[0];
	tmp=inputColor2[1]+((1.0f-inputColor2[1])*valuem);
	if (tmp < inputColor1[1]) outputValue[1]= tmp;
	else outputValue[1] = inputColor1[1];
	tmp=inputColor2[2]+((1.0f-inputColor2[2])*valuem);
	if (tmp < inputColor1[2]) outputValue[2]= tmp;
	else outputValue[2] = inputColor1[2];
	
	outputValue[3] = inputColor1[3];
}

