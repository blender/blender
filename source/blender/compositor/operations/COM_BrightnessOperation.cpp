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

#include "COM_BrightnessOperation.h"

BrightnessOperation::BrightnessOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputProgram = NULL;
}
void BrightnessOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	this->inputBrightnessProgram = this->getInputSocketReader(1);
	this->inputContrastProgram = this->getInputSocketReader(2);
}

void BrightnessOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue[4];
	float a, b;
	float inputBrightness[4];
	float inputContrast[4];
	this->inputProgram->read(inputValue, x, y, sampler, inputBuffers);
	this->inputBrightnessProgram->read(inputBrightness, x, y, sampler, inputBuffers);
	this->inputContrastProgram->read(inputContrast, x, y, sampler, inputBuffers);
	float brightness = inputBrightness[0];
	float contrast = inputContrast[0];
	brightness /= 100.0f;
	float delta = contrast / 200.0f;
	a = 1.0f - delta * 2.0f;
	/*
	* The algorithm is by Werner D. Streidt
	* (http://visca.com/ffactory/archives/5-99/msg00021.html)
	* Extracted of OpenCV demhist.c
	*/
	if( contrast > 0 )
	{
		a = 1.0f / a;
		b = a * (brightness - delta);
	}
	else
	{
		delta *= -1;
		b = a * (brightness + delta);
	}
	
	color[0] = a*inputValue[0]+b;
	color[1] = a*inputValue[1]+b;
	color[2] = a*inputValue[2]+b;
	color[3] = inputValue[3];
}

void BrightnessOperation::deinitExecution() {
	this->inputProgram = NULL;
	this->inputBrightnessProgram = NULL;
	this->inputContrastProgram = NULL;
}

