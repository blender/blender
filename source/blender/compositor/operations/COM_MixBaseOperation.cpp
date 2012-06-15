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

#include "COM_MixBaseOperation.h"

MixBaseOperation::MixBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputValueOperation = NULL;
	this->inputColor1Operation = NULL;
	this->inputColor2Operation = NULL;
	this->setUseValueAlphaMultiply(false);
}

void MixBaseOperation::initExecution()
{
	this->inputValueOperation = this->getInputSocketReader(0);
	this->inputColor1Operation = this->getInputSocketReader(1);
	this->inputColor2Operation = this->getInputSocketReader(2);
}

void MixBaseOperation::executePixel(float *outputColor, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor1[4];
	float inputColor2[4];
	float value;
	
	inputValueOperation->read(&value, x, y, sampler, inputBuffers);
	inputColor1Operation->read(&inputColor1[0], x, y, sampler, inputBuffers);
	inputColor2Operation->read(&inputColor2[0], x, y, sampler, inputBuffers);
	
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	outputColor[0] = valuem * (inputColor1[0]) + value * (inputColor2[0]);
	outputColor[1] = valuem * (inputColor1[1]) + value * (inputColor2[1]);
	outputColor[2] = valuem * (inputColor1[2]) + value * (inputColor2[2]);
	outputColor[3] = inputColor1[3];
}

void MixBaseOperation::deinitExecution()
{
	this->inputValueOperation = NULL;
	this->inputColor1Operation = NULL;
	this->inputColor2Operation = NULL;
}

void MixBaseOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	InputSocket *socket;
	unsigned int tempPreferredResolution[] = {0, 0};
	unsigned int tempResolution[2];
	
	socket = this->getInputSocket(1);
	socket->determineResolution(tempResolution, tempPreferredResolution);
	if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
		this->setResolutionInputSocketIndex(1);
	}
	else {
		socket = this->getInputSocket(2);
		tempPreferredResolution[0] = 0;
		tempPreferredResolution[1] = 0;
		socket->determineResolution(tempResolution, tempPreferredResolution);
		if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
			this->setResolutionInputSocketIndex(2);
		}
		else {
			this->setResolutionInputSocketIndex(0);
		}
	}
	NodeOperation::determineResolution(resolution, preferredResolution);
}

