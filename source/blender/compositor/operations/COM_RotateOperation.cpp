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

#include "COM_RotateOperation.h"
#include "BLI_math.h"

RotateOperation::RotateOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_imageSocket = NULL;
	this->m_degreeSocket =  NULL;
	this->m_doDegree2RadConversion = false;
	this->m_isDegreeSet = false;
}
void RotateOperation::initExecution()
{
	this->m_imageSocket = this->getInputSocketReader(0);
	this->m_degreeSocket = this->getInputSocketReader(1);
	this->m_centerX = this->getWidth() / 2.0;
	this->m_centerY = this->getHeight() / 2.0;
}

void RotateOperation::deinitExecution()
{
	this->m_imageSocket = NULL;
	this->m_degreeSocket = NULL;
}

inline void RotateOperation::ensureDegree()
{
	if (!this->m_isDegreeSet) {
		float degree[4];
		this->m_degreeSocket->read(degree, 0, 0, COM_PS_NEAREST, NULL);
		double rad;
		if (this->m_doDegree2RadConversion) {
			rad = DEG2RAD((double)degree[0]);
		}
		else {
			rad = degree[0];
		}
		this->m_cosine = cos(rad);
		this->m_sine = sin(rad);
		
		this->m_isDegreeSet = true;
	}
}


void RotateOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	ensureDegree();
	const float dy = y - this->m_centerY;
	const float dx = x - this->m_centerX;
	const float nx = this->m_centerX + (this->m_cosine * dx + this->m_sine * dy);
	const float ny = this->m_centerY + (-this->m_sine * dx + this->m_cosine * dy);
	this->m_imageSocket->read(color, nx, ny, sampler, inputBuffers);
}

bool RotateOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	ensureDegree();
	rcti newInput;
	
	const float dxmin = input->xmin - this->m_centerX;
	const float dymin = input->ymin - this->m_centerY;
	const float dxmax = input->xmax - this->m_centerX;
	const float dymax = input->ymax - this->m_centerY;
	
	const float x1 = this->m_centerX + (this->m_cosine * dxmin + this->m_sine * dymin);
	const float x2 = this->m_centerX + (this->m_cosine * dxmax + this->m_sine * dymin);
	const float x3 = this->m_centerX + (this->m_cosine * dxmin + this->m_sine * dymax);
	const float x4 = this->m_centerX + (this->m_cosine * dxmax + this->m_sine * dymax);
	const float y1 = this->m_centerY + (-this->m_sine * dxmin + this->m_cosine * dymin);
	const float y2 = this->m_centerY + (-this->m_sine * dxmax + this->m_cosine * dymin);
	const float y3 = this->m_centerY + (-this->m_sine * dxmin + this->m_cosine * dymax);
	const float y4 = this->m_centerY + (-this->m_sine * dxmax + this->m_cosine * dymax);
	const float minx = min(x1, min(x2, min(x3, x4)));
	const float maxx = max(x1, max(x2, max(x3, x4)));
	const float miny = min(y1, min(y2, min(y3, y4)));
	const float maxy = max(y1, max(y2, max(y3, y4)));
	
	newInput.xmax = ceil(maxx) + 1;
	newInput.xmin = floor(minx) - 1;
	newInput.ymax = ceil(maxy) + 1;
	newInput.ymin = floor(miny) - 1;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
