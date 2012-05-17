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

#include "COM_GlareBaseOperation.h"
#include "BLI_math.h"

GlareBaseOperation::GlareBaseOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->settings = NULL;
	this->cachedInstance = NULL;
	setComplex(true);
}
void GlareBaseOperation::initExecution() {
	initMutex();
	this->inputProgram = getInputSocketReader(0);
	this->cachedInstance = NULL;
}

void GlareBaseOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	float* buffer = (float*) data;
	int index = (y*this->getWidth() + x) * COM_NUMBER_OF_CHANNELS;
	color[0] = buffer[index];
	color[1] = buffer[index+1];
	color[2] = buffer[index+2];
	color[3] = buffer[index+3];
}

void GlareBaseOperation::deinitExecution() {
	deinitMutex();
	this->inputProgram = NULL;
	if (this->cachedInstance) {
		delete cachedInstance;
		this->cachedInstance = NULL;
	}
}
void* GlareBaseOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	BLI_mutex_lock(getMutex());
	if (this->cachedInstance == NULL) {
		MemoryBuffer* tile = (MemoryBuffer*)inputProgram->initializeTileData(rect, memoryBuffers);
		float* data = new float[this->getWidth()*this->getHeight()*COM_NUMBER_OF_CHANNELS];
		this->generateGlare(data, tile, this->settings);
		this->cachedInstance = data;
	}
	BLI_mutex_unlock(getMutex());
	return this->cachedInstance;
}

bool GlareBaseOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;
	newInput.xmax = this->getWidth();
	newInput.xmin = 0;
	newInput.ymax = this->getHeight();
	newInput.ymin = 0;
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
