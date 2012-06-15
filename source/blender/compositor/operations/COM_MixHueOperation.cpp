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

#include "COM_MixHueOperation.h"

extern "C" {
	#include "BLI_math.h"
}

MixHueOperation::MixHueOperation(): MixBaseOperation()
{
	/* pass */
}

void MixHueOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor1[4];
	float inputColor2[4];
	float value;

	inputValueOperation->read(&value, x, y, sampler, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, sampler, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, sampler, inputBuffers);

	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	
	float colH,colS,colV;
	rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
	if (colS!=0.0f) {
		float rH,rS,rV;
		float tmpr,tmpg,tmpb;
		rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
		hsv_to_rgb(colH , rS, rV, &tmpr, &tmpg, &tmpb);
		outputValue[0] = valuem*(inputColor1[0]) + value*tmpr;
		outputValue[1] = valuem*(inputColor1[1]) + value*tmpg;
		outputValue[2] = valuem*(inputColor1[2]) + value*tmpb;
	}
	outputValue[3] = inputColor1[3];
}
