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

#include "COM_ConvertVectorToValueOperation.h"

ConvertVectorToValueOperation::ConvertVectorToValueOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_VECTOR);
	this->addOutputSocket(COM_DT_VALUE);
	this->inputOperation = NULL;
}

void ConvertVectorToValueOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertVectorToValueOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float input[4];
	inputOperation->read(input, x, y, sampler, inputBuffers);
	outputValue[0] = (input[0]+input[1]+input[2])/3.0f;
}

void ConvertVectorToValueOperation::deinitExecution() {
	this->inputOperation = NULL;
}
