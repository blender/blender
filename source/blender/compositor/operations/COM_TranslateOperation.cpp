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

#include "COM_TranslateOperation.h"

TranslateOperation::TranslateOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}
void TranslateOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
	this->inputXOperation = this->getInputSocketReader(1);
	this->inputYOperation = this->getInputSocketReader(2);

	float tempDelta[4];
	this->inputXOperation->read(tempDelta, 0, 0, COM_PS_NEAREST, NULL);
	this->deltaX = tempDelta[0];
	this->inputYOperation->read(tempDelta, 0, 0, COM_PS_NEAREST, NULL);
	this->deltaY = tempDelta[0];
}

void TranslateOperation::deinitExecution() {
	this->inputOperation = NULL;
	this->inputXOperation = NULL;
	this->inputYOperation = NULL;
}


void TranslateOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	this->inputOperation->read(color, x-this->getDeltaX(), y-this->getDeltaY(), sampler, inputBuffers);
}

bool TranslateOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	
	newInput.xmax = input->xmax - this->getDeltaX();
	newInput.xmin = input->xmin - this->getDeltaX();
	newInput.ymax = input->ymax - this->getDeltaY();
	newInput.ymin = input->ymin - this->getDeltaY();
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
