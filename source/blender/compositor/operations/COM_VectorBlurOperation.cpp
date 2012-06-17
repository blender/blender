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

#include "COM_VectorBlurOperation.h"
#include "BLI_math.h"

// use the implementation of blender internal renderer to calculate the vector blur.
extern "C" {
	#include "RE_pipeline.h"
}

VectorBlurOperation::VectorBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE); // ZBUF
	this->addInputSocket(COM_DT_COLOR); //SPEED
	this->addOutputSocket(COM_DT_COLOR);
	this->settings = NULL;
	this->cachedInstance = NULL;
	this->inputImageProgram = NULL;
	this->inputSpeedProgram = NULL;
	this->inputZProgram = NULL;
	setComplex(true);
}
void VectorBlurOperation::initExecution()
{
	initMutex();
	this->inputImageProgram = getInputSocketReader(0);
	this->inputZProgram = getInputSocketReader(1);
	this->inputSpeedProgram = getInputSocketReader(2);
	this->cachedInstance = NULL;
	QualityStepHelper::initExecution(COM_QH_INCREASE);
	
}

void VectorBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float *buffer = (float *) data;
	int index = (y * this->getWidth() + x) * COM_NUMBER_OF_CHANNELS;
	color[0] = buffer[index];
	color[1] = buffer[index + 1];
	color[2] = buffer[index + 2];
	color[3] = buffer[index + 3];
}

void VectorBlurOperation::deinitExecution()
{
	deinitMutex();
	this->inputImageProgram = NULL;
	this->inputSpeedProgram = NULL;
	this->inputZProgram = NULL;
	if (this->cachedInstance) {
		delete cachedInstance;
		this->cachedInstance = NULL;
	}
}
void *VectorBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->cachedInstance) return this->cachedInstance;
	
	lockMutex();
	if (this->cachedInstance == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)inputImageProgram->initializeTileData(rect, memoryBuffers);
		MemoryBuffer *speed = (MemoryBuffer *)inputSpeedProgram->initializeTileData(rect, memoryBuffers);
		MemoryBuffer *z = (MemoryBuffer *)inputZProgram->initializeTileData(rect, memoryBuffers);
		float *data = new float[this->getWidth() * this->getHeight() * COM_NUMBER_OF_CHANNELS];
		memcpy(data, tile->getBuffer(), this->getWidth() * this->getHeight() * COM_NUMBER_OF_CHANNELS * sizeof(float));
		this->generateVectorBlur(data, tile, speed, z);
		this->cachedInstance = data;
	}
	unlockMutex();
	return this->cachedInstance;
}

bool VectorBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->cachedInstance == NULL) {
		rcti newInput;
		newInput.xmax = this->getWidth();
		newInput.xmin = 0;
		newInput.ymax = this->getHeight();
		newInput.ymin = 0;
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
	else {
		return false;
	}
}

void VectorBlurOperation::generateVectorBlur(float *data, MemoryBuffer *inputImage, MemoryBuffer *inputSpeed, MemoryBuffer *inputZ)
{
	float *zbuf = inputZ->convertToValueBuffer();
	NodeBlurData blurdata;
	blurdata.samples = this->settings->samples / QualityStepHelper::getStep();
	blurdata.maxspeed = this->settings->maxspeed;
	blurdata.minspeed = this->settings->minspeed;
	blurdata.curved = this->settings->curved;
	blurdata.fac = this->settings->fac;
	RE_zbuf_accumulate_vecblur(&blurdata, this->getWidth(), this->getHeight(), data, inputImage->getBuffer(), inputSpeed->getBuffer(), zbuf);
	delete zbuf;
	return;
}
