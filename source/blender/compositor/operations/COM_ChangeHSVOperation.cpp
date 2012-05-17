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

#include "COM_ChangeHSVOperation.h"

ChangeHSVOperation::ChangeHSVOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputOperation = NULL;
}

void ChangeHSVOperation::initExecution() {
	this->inputOperation = getInputSocketReader(0);
}

void ChangeHSVOperation::deinitExecution() {
	this->inputOperation = NULL;
}

void ChangeHSVOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputColor1[4];
	
	inputOperation->read(inputColor1, x, y, sampler, inputBuffers);
	
	outputValue[0] = inputColor1[0] + (this->hue - 0.5f);
	if (outputValue[0]>1.0f) outputValue[0]-=1.0; else if(outputValue[0]<0.0) outputValue[0]+= 1.0;
	outputValue[1] = inputColor1[1] * this->saturation;
	outputValue[2] = inputColor1[2] * this->value;
	outputValue[3] = inputColor1[3];
}

