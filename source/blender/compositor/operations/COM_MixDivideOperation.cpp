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

#include "COM_MixDivideOperation.h"

MixDivideOperation::MixDivideOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDivideOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float value;
	
	this->m_inputValueOperation->read(&value, x, y, sampler);
	this->m_inputColor1Operation->read(&inputColor1[0], x, y, sampler);
	this->m_inputColor2Operation->read(&inputColor2[0], x, y, sampler);
	
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	
	if (inputColor2[0] != 0.0f)
		outputValue[0] = valuem * (inputColor1[0]) + value * (inputColor1[0]) / inputColor2[0];
	else
		outputValue[0] = 0.0f;
	if (inputColor2[1] != 0.0f)
		outputValue[1] = valuem * (inputColor1[1]) + value * (inputColor1[1]) / inputColor2[1];
	else
		outputValue[1] = 0.0f;
	if (inputColor2[2] != 0.0f)
		outputValue[2] = valuem * (inputColor1[2]) + value * (inputColor1[2]) / inputColor2[2];
	else
		outputValue[2] = 0.0f;
	
	outputValue[3] = inputColor1[3];

	clampIfNeeded(outputValue);
}

