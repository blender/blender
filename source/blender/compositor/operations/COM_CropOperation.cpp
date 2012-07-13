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
	
	this->m_xmax = MAX2(this->m_settings->x1, this->m_settings->x2) + 1;
	this->m_xmin = MIN2(this->m_settings->x1, this->m_settings->x2);
	this->m_ymax = MAX2(this->m_settings->y1, this->m_settings->y2) + 1;
	this->m_ymin = MIN2(this->m_settings->y1, this->m_settings->y2);
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

void CropOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	if ((x < this->m_xmax && x >= this->m_xmin) && (y < this->m_ymax && y >= this->m_ymin)) {
		this->m_inputOperation->read(color, x, y, sampler);
	}
	else {
		zero_v4(color);
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

void CropImageOperation::determineResolution(unsigned int resolution[], unsigned int preferedResolution[])
{
	NodeOperation::determineResolution(resolution, preferedResolution);
	updateArea();
	resolution[0] = this->m_xmax - this->m_xmin;
	resolution[1] = this->m_ymax - this->m_ymin;
}

void CropImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	this->m_inputOperation->read(color, (x + this->m_xmin), (y + this->m_ymin), sampler);
}
