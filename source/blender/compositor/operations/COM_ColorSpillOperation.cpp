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
#define avg(a,b) ((a+b)/2)

ColorSpillOperation::ColorSpillOperation(): NodeOperation() {
	addInputSocket(COM_DT_COLOR);
	addInputSocket(COM_DT_VALUE);
	addOutputSocket(COM_DT_COLOR);

	inputImageReader = NULL;
	inputFacReader = NULL;
	this->spillChannel = 1; // GREEN
}

void ColorSpillOperation::initExecution() {
	this->inputImageReader = this->getInputSocketReader(0);
	this->inputFacReader = this->getInputSocketReader(1);
	if (spillChannel == 0) {
		rmut = -1.0f;
		gmut = 1.0f;
		bmut = 1.0f;
		this->channel2 = 1;
		this->channel3 = 2;
		if (settings->unspill == 0) {
			settings->uspillr = 1.0f;
			settings->uspillg = 0.0f;
			settings->uspillb = 0.0f;
		}
	} else if (spillChannel == 1) {
		rmut = 1.0f;
		gmut = -1.0f;
		bmut = 1.0f;
		this->channel2 = 0;
		this->channel3 = 2;
		if (settings->unspill == 0) {
			settings->uspillr = 0.0f;
			settings->uspillg = 1.0f;
			settings->uspillb = 0.0f;
		}
	} else {
		rmut = 1.0f;
		gmut = 1.0f;
		bmut = -1.0f;
		
		this->channel2 = 0;
		this->channel3 = 1;
		if (settings->unspill == 0) {
			settings->uspillr = 0.0f;
			settings->uspillg = 0.0f;
			settings->uspillb = 1.0f;
		}
	}
}

void ColorSpillOperation::deinitExecution() {
	this->inputImageReader= NULL;
	this->inputFacReader = NULL;
}

void ColorSpillOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float fac[4];
	float input[4];
	float map;	
	this->inputFacReader->read(fac, x, y, sampler, inputBuffers);
	this->inputImageReader->read(input, x, y, sampler, inputBuffers);
	float rfac = min(1.0f, fac[0]);
	map = calculateMapValue(rfac, input);
	if(map>0) {
		outputValue[0]=input[0]+rmut*(settings->uspillr*map);
		outputValue[1]=input[1]+gmut*(settings->uspillg*map);
		outputValue[2]=input[2]+bmut*(settings->uspillb*map);
		outputValue[3]=input[3];
	}
	else {
		outputValue[0]=input[0];
		outputValue[1]=input[1];
		outputValue[2]=input[2];
		outputValue[3]=input[3];
	}	
}
float ColorSpillOperation::calculateMapValue(float fac, float *input) {
	return fac * (input[this->spillChannel]-(this->settings->limscale*input[settings->limchan]));
}


float ColorSpillAverageOperation::calculateMapValue(float fac, float *input) {
	return fac * (input[this->spillChannel]-(this->settings->limscale*avg(input[this->channel2], input[this->channel3])));
}
