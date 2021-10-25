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

ProjectorLensDistortionOperation::ProjectorLensDistortionOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputProgram = NULL;
	this->m_dispersionAvailable = false;
	this->m_dispersion = 0.0f;
}
void ProjectorLensDistortionOperation::initExecution()
{
	this->initMutex();
	this->m_inputProgram = this->getInputSocketReader(0);
}

void *ProjectorLensDistortionOperation::initializeTileData(rcti * /*rect*/)
{
	updateDispersion();
	void *buffer = this->m_inputProgram->initializeTileData(NULL);
	return buffer;
}

void ProjectorLensDistortionOperation::executePixel(float output[4], int x, int y, void *data)
{
	float inputValue[4];
	const float height = this->getHeight();
	const float width = this->getWidth();
	const float v = (y + 0.5f) / height;
	const float u = (x + 0.5f) / width;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	inputBuffer->readBilinear(inputValue, (u * width + this->m_kr2) - 0.5f, v * height - 0.5f);
	output[0] = inputValue[0];
	inputBuffer->read(inputValue, x, y);
	output[1] = inputValue[1];
	inputBuffer->readBilinear(inputValue, (u * width - this->m_kr2) - 0.5f, v * height - 0.5f);
	output[2] = inputValue[2];
	output[3] = 1.0f;
}

void ProjectorLensDistortionOperation::deinitExecution()
{
	this->deinitMutex();
	this->m_inputProgram = NULL;
}

bool ProjectorLensDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	if (this->m_dispersionAvailable) {
		newInput.ymax = input->ymax;
		newInput.ymin = input->ymin;
		newInput.xmin = input->xmin - this->m_kr2 - 2;
		newInput.xmax = input->xmax + this->m_kr2 + 2;
	}
	else {
		rcti dispInput;
		BLI_rcti_init(&dispInput, 0, 5, 0, 5);
		if (this->getInputOperation(1)->determineDependingAreaOfInterest(&dispInput, readOperation, output)) {
			return true;
		}
		newInput.xmin = input->xmin - 7;  /* (0.25f * 20 * 1) + 2 == worse case dispersion */
		newInput.ymin = input->ymin;
		newInput.ymax = input->ymax;
		newInput.xmax = input->xmax + 7;  /* (0.25f * 20 * 1) + 2 == worse case dispersion */
	}
	if (this->getInputOperation(0)->determineDependingAreaOfInterest(&newInput, readOperation, output)) {
		return true;
	}
	return false;
}

void ProjectorLensDistortionOperation::updateDispersion() 
{
	if (this->m_dispersionAvailable) return;
	this->lockMutex();
	if (!this->m_dispersionAvailable) {
		float result[4];
		this->getInputSocketReader(1)->readSampled(result, 1, 1, COM_PS_NEAREST);
		this->m_dispersion = result[0];
		this->m_kr = 0.25f * max_ff(min_ff(this->m_dispersion, 1.0f), 0.0f);
		this->m_kr2 = this->m_kr * 20;
		this->m_dispersionAvailable = true;
	}
	this->unlockMutex();
}
