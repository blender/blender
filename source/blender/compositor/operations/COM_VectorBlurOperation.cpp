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

#include "MEM_guardedalloc.h"

// use the implementation of blender internal renderer to calculate the vector blur.
extern "C" {
#  include "RE_pipeline.h"
}

VectorBlurOperation::VectorBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE); // ZBUF
	this->addInputSocket(COM_DT_COLOR); //SPEED
	this->addOutputSocket(COM_DT_COLOR);
	this->m_settings = NULL;
	this->m_cachedInstance = NULL;
	this->m_inputImageProgram = NULL;
	this->m_inputSpeedProgram = NULL;
	this->m_inputZProgram = NULL;
	setComplex(true);
}
void VectorBlurOperation::initExecution()
{
	initMutex();
	this->m_inputImageProgram = getInputSocketReader(0);
	this->m_inputZProgram = getInputSocketReader(1);
	this->m_inputSpeedProgram = getInputSocketReader(2);
	this->m_cachedInstance = NULL;
	QualityStepHelper::initExecution(COM_QH_INCREASE);
	
}

void VectorBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	float *buffer = (float *) data;
	int index = (y * this->getWidth() + x) * COM_NUM_CHANNELS_COLOR;
	copy_v4_v4(output, &buffer[index]);
}

void VectorBlurOperation::deinitExecution()
{
	deinitMutex();
	this->m_inputImageProgram = NULL;
	this->m_inputSpeedProgram = NULL;
	this->m_inputZProgram = NULL;
	if (this->m_cachedInstance) {
		MEM_freeN(this->m_cachedInstance);
		this->m_cachedInstance = NULL;
	}
}
void *VectorBlurOperation::initializeTileData(rcti *rect)
{
	if (this->m_cachedInstance) {
		return this->m_cachedInstance;
	}
	
	lockMutex();
	if (this->m_cachedInstance == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_inputImageProgram->initializeTileData(rect);
		MemoryBuffer *speed = (MemoryBuffer *)this->m_inputSpeedProgram->initializeTileData(rect);
		MemoryBuffer *z = (MemoryBuffer *)this->m_inputZProgram->initializeTileData(rect);
		float *data = (float *)MEM_dupallocN(tile->getBuffer());
		this->generateVectorBlur(data, tile, speed, z);
		this->m_cachedInstance = data;
	}
	unlockMutex();
	return this->m_cachedInstance;
}

bool VectorBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->m_cachedInstance == NULL) {
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
	NodeBlurData blurdata;
	blurdata.samples = this->m_settings->samples / QualityStepHelper::getStep();
	blurdata.maxspeed = this->m_settings->maxspeed;
	blurdata.minspeed = this->m_settings->minspeed;
	blurdata.curved = this->m_settings->curved;
	blurdata.fac = this->m_settings->fac;
	RE_zbuf_accumulate_vecblur(&blurdata, this->getWidth(), this->getHeight(), data, inputImage->getBuffer(), inputSpeed->getBuffer(), inputZ->getBuffer());
	return;
}
