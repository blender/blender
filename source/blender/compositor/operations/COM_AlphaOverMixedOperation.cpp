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

#include "COM_AlphaOverMixedOperation.h"

AlphaOverMixedOperation::AlphaOverMixedOperation() : MixBaseOperation()
{
	this->m_x = 0.0f;
}

void AlphaOverMixedOperation::executePixel(float outputValue[4], float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor1[4];
	float inputOverColor[4];
	float value[4];
	
	this->m_inputValueOperation->read(value, x, y, sampler, inputBuffers);
	this->m_inputColor1Operation->read(inputColor1, x, y, sampler, inputBuffers);
	this->m_inputColor2Operation->read(inputOverColor, x, y, sampler, inputBuffers);
	
	if (inputOverColor[3] <= 0.0f) {
		copy_v4_v4(outputValue, inputColor1);
	}
	else if (value[0] == 1.0f && inputOverColor[3] >= 1.0f) {
		copy_v4_v4(outputValue, inputOverColor);
	}
	else {
		float addfac = 1.0f - this->m_x + inputOverColor[3] * this->m_x;
		float premul = value[0] * addfac;
		float mul = 1.0f - value[0] * inputOverColor[3];

		outputValue[0] = (mul * inputColor1[0]) + premul * inputOverColor[0];
		outputValue[1] = (mul * inputColor1[1]) + premul * inputOverColor[1];
		outputValue[2] = (mul * inputColor1[2]) + premul * inputOverColor[2];
		outputValue[3] = (mul * inputColor1[3]) + value[0] * inputOverColor[3];
	}
}

