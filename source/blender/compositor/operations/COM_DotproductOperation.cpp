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

#include "COM_DotproductOperation.h"

DotproductOperation::DotproductOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_VECTOR);
	this->addInputSocket(COM_DT_VECTOR);
	this->addOutputSocket(COM_DT_VALUE);
	this->setResolutionInputSocketIndex(0);
	this->input1Operation = NULL;
	this->input2Operation = NULL;
}
void DotproductOperation::initExecution() {
	this->input1Operation = this->getInputSocketReader(0);
	this->input2Operation = this->getInputSocketReader(1);
}

void DotproductOperation::deinitExecution() {
	this->input1Operation = NULL;
	this->input2Operation = NULL;
}

/** @todo: current implementation is the inverse of a dotproduct. not 'logically' correct
  */
void DotproductOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float input1[4];
	float input2[4];
	this->input1Operation->read(input1, x, y, sampler, inputBuffers);
	this->input2Operation->read(input2, x, y, sampler, inputBuffers);
	color[0] = -(input1[0]*input2[0]+input1[1]*input2[1]+input1[2]*input2[2]);
}
