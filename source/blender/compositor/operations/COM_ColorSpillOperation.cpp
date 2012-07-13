/*
 * Copyright 2011, Blender Foundation.
 *
 * This Reader is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This Reader is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this Reader; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 */

#include "COM_ColorSpillOperation.h"
#include "BLI_math.h"
#define AVG(a, b) ((a + b) / 2)

ColorSpillOperation::ColorSpillOperation() : NodeOperation()
{
	addInputSocket(COM_DT_COLOR);
	addInputSocket(COM_DT_VALUE);
	addOutputSocket(COM_DT_COLOR);

	this->m_inputImageReader = NULL;
	this->m_inputFacReader = NULL;
	this->m_spillChannel = 1; // GREEN
}

void ColorSpillOperation::initExecution()
{
	this->m_inputImageReader = this->getInputSocketReader(0);
	this->m_inputFacReader = this->getInputSocketReader(1);
	if (this->m_spillChannel == 0) {
		this->m_rmut = -1.0f;
		this->m_gmut = 1.0f;
		this->m_bmut = 1.0f;
		this->m_channel2 = 1;
		this->m_channel3 = 2;
		if (this->m_settings->unspill == 0) {
			this->m_settings->uspillr = 1.0f;
			this->m_settings->uspillg = 0.0f;
			this->m_settings->uspillb = 0.0f;
		}
	}
	else if (this->m_spillChannel == 1) {
		this->m_rmut = 1.0f;
		this->m_gmut = -1.0f;
		this->m_bmut = 1.0f;
		this->m_channel2 = 0;
		this->m_channel3 = 2;
		if (this->m_settings->unspill == 0) {
			this->m_settings->uspillr = 0.0f;
			this->m_settings->uspillg = 1.0f;
			this->m_settings->uspillb = 0.0f;
		}
	}
	else {
		this->m_rmut = 1.0f;
		this->m_gmut = 1.0f;
		this->m_bmut = -1.0f;
		
		this->m_channel2 = 0;
		this->m_channel3 = 1;
		if (this->m_settings->unspill == 0) {
			this->m_settings->uspillr = 0.0f;
			this->m_settings->uspillg = 0.0f;
			this->m_settings->uspillb = 1.0f;
		}
	}
}

void ColorSpillOperation::deinitExecution()
{
	this->m_inputImageReader = NULL;
	this->m_inputFacReader = NULL;
}

void ColorSpillOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float fac[4];
	float input[4];
	this->m_inputFacReader->read(fac, x, y, sampler);
	this->m_inputImageReader->read(input, x, y, sampler);
	float rfac = min(1.0f, fac[0]);
	float map = calculateMapValue(rfac, input);
	if (map > 0.0f) {
		outputValue[0] = input[0] + this->m_rmut * (this->m_settings->uspillr * map);
		outputValue[1] = input[1] + this->m_gmut * (this->m_settings->uspillg * map);
		outputValue[2] = input[2] + this->m_bmut * (this->m_settings->uspillb * map);
		outputValue[3] = input[3];
	}
	else {
		copy_v4_v4(outputValue, input);
	}	
}
float ColorSpillOperation::calculateMapValue(float fac, float *input)
{
	return fac * (input[this->m_spillChannel] - (this->m_settings->limscale * input[this->m_settings->limchan]));
}


float ColorSpillAverageOperation::calculateMapValue(float fac, float *input)
{
	return fac * (input[this->m_spillChannel] - (this->m_settings->limscale * AVG(input[this->m_channel2], input[this->m_channel3])));
}
