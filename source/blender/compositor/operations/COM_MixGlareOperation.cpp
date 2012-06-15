/*
 * Copyright 2011, Glareer Foundation.
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

#include "COM_MixGlareOperation.h"

MixGlareOperation::MixGlareOperation(): MixBaseOperation()
{
	/* pass */
}

void MixGlareOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float value;
	
	inputValueOperation->read(inputValue, x, y, sampler, inputBuffers);
	inputColor1Operation->read(inputColor1, x, y, sampler, inputBuffers);
	inputColor2Operation->read(inputColor2, x, y, sampler, inputBuffers);
	value = inputValue[0];
	float mf = 2.f - 2.f*fabsf(value - 0.5f);
	
	outputValue[0] = mf*((inputColor1[0])+value*(inputColor2[0]-inputColor1[0]));
	outputValue[1] = mf*((inputColor1[1])+value*(inputColor2[1]-inputColor1[1]));
	outputValue[2] = mf*((inputColor1[2])+value*(inputColor2[2]-inputColor1[2]));
	outputValue[3] = inputColor1[3];
}
