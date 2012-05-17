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

#include "COM_MathBaseOperation.h"
extern "C" {
#include "BLI_math.h"
}

MathBaseOperation::MathBaseOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->inputValue1Operation = NULL;
	this->inputValue2Operation = NULL;
}

void MathBaseOperation::initExecution() {
	this->inputValue1Operation = this->getInputSocketReader(0);
	this->inputValue2Operation = this->getInputSocketReader(1);
}


void MathBaseOperation::deinitExecution() {
	this->inputValue1Operation = NULL;
	this->inputValue2Operation = NULL;
}

void MathAddOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = inputValue1[0] + inputValue2[0];
}

void MathSubtractOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = inputValue1[0] - inputValue2[0];
}

void MathMultiplyOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = inputValue1[0] * inputValue2[0];
}

void MathDivideOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	if(inputValue2[0]==0)	/* We don't want to divide by zero. */
		outputValue[0]= 0.0;
	else
		outputValue[0]= inputValue1[0] / inputValue2[0];
}

void MathSineOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = sin(inputValue1[0]);
}

void MathCosineOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = cos(inputValue1[0]);
}

void MathTangentOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = tan(inputValue1[0]);
}

void MathArcSineOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	if(inputValue1[0] <= 1 && inputValue1[0] >= -1 )
		outputValue[0]= asin(inputValue1[0]);
	else
		outputValue[0]= 0.0;
}

void MathArcCosineOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	if(inputValue1[0] <= 1 && inputValue1[0] >= -1 )
		outputValue[0]= acos(inputValue1[0]);
	else
		outputValue[0]= 0.0;
}

void MathArcTangentOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = atan(inputValue1[0]);
}

void MathPowerOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	if( inputValue1[0] >= 0 ) {
		outputValue[0]= pow(inputValue1[0], inputValue2[0]);
	} else {
		float y_mod_1 = fmod(inputValue2[0], 1);
		/* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
		if (y_mod_1 > 0.999 || y_mod_1 < 0.001) {
			outputValue[0]= pow(inputValue1[0], (float)floor(inputValue2[0] + 0.5));
		} else {
			outputValue[0] = 0.0;
		}
	}
}

void MathLogarithmOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	if( inputValue1[0] > 0  && inputValue2[0] > 0 )
		outputValue[0]= log(inputValue1[0]) / log(inputValue2[0]);
	else
		outputValue[0]= 0.0;
}

void MathMinimumOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = min(inputValue1[0], inputValue2[0]);
}

void MathMaximumOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = max(inputValue1[0], inputValue2[0]);
}

void MathRoundOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = round(inputValue1[0]);
}

void MathLessThanOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = inputValue1[0]<inputValue2[0]?1.0f:0.0f;
}

void MathGreaterThanOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputValue1[4];
	float inputValue2[4];
	
	inputValue1Operation->read(&inputValue1[0], x, y, sampler, inputBuffers);
	inputValue2Operation->read(&inputValue2[0], x, y, sampler, inputBuffers);
	
	outputValue[0] = inputValue1[0]>inputValue2[0]?1.0f:0.0f;
}


