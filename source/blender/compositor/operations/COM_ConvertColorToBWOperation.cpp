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

#include "COM_ConvertColorToBWOperation.h"

ConvertColorToBWOperation::ConvertColorToBWOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_VALUE);
	this->inputOperation = NULL;
}

void ConvertColorToBWOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertColorToBWOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor[4];
	inputOperation->read(&inputColor[0], x, y, sampler, inputBuffers);
	outputValue[0] = rgb_to_bw(inputColor);
}

void ConvertColorToBWOperation::deinitExecution()
{
	this->inputOperation = NULL;
}
