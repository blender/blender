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

#include "COM_CombineChannelsOperation.h"
#include <stdio.h>

CombineChannelsOperation::CombineChannelsOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputChannel1Operation = NULL;
	this->inputChannel2Operation = NULL;
	this->inputChannel3Operation = NULL;
	this->inputChannel4Operation = NULL;
}
void CombineChannelsOperation::initExecution() {
	this->inputChannel1Operation = this->getInputSocketReader(0);
	this->inputChannel2Operation = this->getInputSocketReader(1);
	this->inputChannel3Operation = this->getInputSocketReader(2);
	this->inputChannel4Operation = this->getInputSocketReader(3);
}

void CombineChannelsOperation::deinitExecution() {
	this->inputChannel1Operation = NULL;
	this->inputChannel2Operation = NULL;
	this->inputChannel3Operation = NULL;
	this->inputChannel4Operation = NULL;
}


void CombineChannelsOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float input[4];
	/// @todo: remove if statements
	if (this->inputChannel1Operation) {
		this->inputChannel1Operation->read(input, x, y, sampler, inputBuffers);
		color[0] = input[0];
	}
	if (this->inputChannel2Operation) {
		this->inputChannel2Operation->read(input, x, y, sampler, inputBuffers);
		color[1] = input[0];
	}
	if (this->inputChannel3Operation) {
		this->inputChannel3Operation->read(input, x, y, sampler, inputBuffers);
		color[2] = input[0];
	}
	if (this->inputChannel4Operation) {
		this->inputChannel4Operation->read(input, x, y, sampler, inputBuffers);
		color[3] = input[0];
	}
}
