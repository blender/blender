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

CropBaseOperation::CropBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputOperation = NULL;
	this->m_settings = NULL;
}

void CropBaseOperation::updateArea()
{
	SocketReader *inputReference = this->getInputSocketReader(0);
	float width = inputReference->getWidth();
	float height = inputReference->getHeight();
	
	if (width > 0.0f && height > 0.0f) {
		if (this->m_relative) {
			this->m_settings->x1 = width * this->m_settings->fac_x1;
			this->m_settings->x2 = width * this->m_settings->fac_x2;
			this->m_settings->y1 = height * this->m_settings->fac_y1;
			this->m_settings->y2 = height * this->m_settings->fac_y2;
		}
		if (width <= this->m_settings->x1 + 1)
			this->m_settings->x1 = width - 1;
		if (height <= this->m_settings->y1 + 1)
			this->m_settings->y1 = height - 1;
		if (width <= this->m_settings->x2 + 1)
			this->m_settings->x2 = width - 1;
		if (height <= this->m_settings->y2 + 1)
			this->m_settings->y2 = height - 1;
		
		this->m_xmax = max(this->m_settings->x1, this->m_settings->x2) + 1;
		this->m_xmin = min(this->m_settings->x1, this->m_settings->x2);
		this->m_ymax = max(this->m_settings->y1, this->m_settings->y2) + 1;
		this->m_ymin = min(this->m_settings->y1, this->m_settings->y2);
	}
	else {
		this->m_xmax = 0;
		this->m_xmin = 0;
		this->m_ymax = 0;
		this->m_ymin = 0;
	}
}

void CropBaseOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	updateArea();
}

void CropBaseOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}

CropOperation::CropOperation() : CropBaseOperation()
{
	/* pass */
}

void CropOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if ((x < this->m_xmax && x >= this->m_xmin) && (y < this->m_ymax && y >= this->m_ymin)) {
		this->m_inputOperation->readSampled(output, x, y, sampler);
	}
	else {
		zero_v4(output);
	}
}

CropImageOperation::CropImageOperation() : CropBaseOperation()
{
	/* pass */
}

bool CropImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
	newInput.xmax = input->xmax + this->m_xmin;
	newInput.xmin = input->xmin + this->m_xmin;
	newInput.ymax = input->ymax + this->m_ymin;
	newInput.ymin = input->ymin + this->m_ymin;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void CropImageOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	NodeOperation::determineResolution(resolution, preferredResolution);
	updateArea();
	resolution[0] = this->m_xmax - this->m_xmin;
	resolution[1] = this->m_ymax - this->m_ymin;
}

void CropImageOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (x >= 0 && x < getWidth() && y >= 0 && y < getHeight()) {
		this->m_inputOperation->readSampled(output, (x + this->m_xmin), (y + this->m_ymin), sampler);
	}
	else {
		zero_v4(output);
	}
}
