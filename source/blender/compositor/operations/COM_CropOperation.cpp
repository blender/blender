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

#include "COM_CropOperation.h"
#include "BLI_math.h"

CropBaseOperation::CropBaseOperation() :NodeOperation() {
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputOperation = NULL;
	this->settings = NULL;
}

void CropBaseOperation::updateArea() {
	SocketReader * inputReference = this->getInputSocketReader(0);
	float width = inputReference->getWidth();
	float height = inputReference->getHeight();
	if (this->relative) {
		settings->x1= width * settings->fac_x1;
		settings->x2= width * settings->fac_x2;
		settings->y1= height * settings->fac_y1;
		settings->y2= height * settings->fac_y2;
	}
	if (width <= settings->x1 + 1)
		settings->x1 = width - 1;
	if (height <= settings->y1 + 1)
		settings->y1 = height - 1;
	if (width <= settings->x2 + 1)
		settings->x2 = width - 1;
	if (height <= settings->y2 + 1)
		settings->y2 = height - 1;
	
	this->xmax = MAX2(settings->x1, settings->x2) + 1;
	this->xmin = MIN2(settings->x1, settings->x2);
	this->ymax = MAX2(settings->y1, settings->y2) + 1;
	this->ymin = MIN2(settings->y1, settings->y2);
}

void CropBaseOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
	updateArea();
}

void CropBaseOperation::deinitExecution() {
	this->inputOperation = NULL;
}

CropOperation::CropOperation() :CropBaseOperation() {
}

void CropOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	if ((x < this->xmax && x >= xmin) && (y < ymax && y >= ymin)) {
		inputOperation->read(color, x, y, sampler, inputBuffers);
	}
	else {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.0f;
	}
}

CropImageOperation::CropImageOperation() :CropBaseOperation() {
}

bool CropImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	
	newInput.xmax = input->xmax + this->xmin;
	newInput.xmin = input->xmin + this->xmin;
	newInput.ymax = input->ymax + this->ymin;
	newInput.ymin = input->ymin + this->ymin;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void CropImageOperation::determineResolution(unsigned int resolution[], unsigned int preferedResolution[]) {
	NodeOperation::determineResolution(resolution, preferedResolution);
	updateArea();
	resolution[0] = this->xmax - this->xmin;
	resolution[1] = this->ymax - this->ymin;
}

void CropImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	this->inputOperation->read(color, (x + this->xmin), (y + this->ymin), sampler, inputBuffers);
}
