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

#include "COM_ProjectorLensDistortionOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

ProjectorLensDistortionOperation::ProjectorLensDistortionOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->inputProgram = NULL;
}
void ProjectorLensDistortionOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	kr = 0.25f*MAX2(MIN2(this->dispersion, 1.f), 0.f);
	kr2 = kr * 20;
}

void* ProjectorLensDistortionOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	void* buffer = inputProgram->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void ProjectorLensDistortionOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	float inputValue[4];
	const float height = this->getHeight();
	const float width = this->getWidth();
	const float v = (y + 0.5f)/height;
	const float u = (x + 0.5f)/width;
	MemoryBuffer * inputBuffer = (MemoryBuffer*)data;
	inputBuffer->readCubic(inputValue, (u*width + kr2) - 0.5f, v*height - 0.5f);
	color[0] = inputValue[0];
	inputBuffer->read(inputValue, x, y);
	color[1] = inputValue[1];
	inputBuffer->readCubic(inputValue, (u*width - kr2) - 0.5f, v*height - 0.5f);
	color[2] = inputValue[2];
	color[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinitExecution() {
	this->inputProgram = NULL;
}

bool ProjectorLensDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	newInput.ymax = input->ymax;
	newInput.ymin = input->ymin;
	newInput.xmin = input->xmin-kr2-2;
	newInput.xmax = input->xmax+kr2+2;
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
