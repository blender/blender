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
	this->imageSocket = NULL;
	this->degreeSocket =  NULL;
	this->doDegree2RadConversion = false;
	this->isDegreeSet = false;
}
void RotateOperation::initExecution()
{
	this->imageSocket = this->getInputSocketReader(0);
	this->degreeSocket = this->getInputSocketReader(1);
	this->centerX = this->getWidth() / 2.0;
	this->centerY = this->getHeight() / 2.0;
}

void RotateOperation::deinitExecution()
{
	this->imageSocket = NULL;
	this->degreeSocket = NULL;
}

inline void RotateOperation::ensureDegree()
{
	if (!isDegreeSet) {
		float degree[4];
		this->degreeSocket->read(degree, 0, 0, COM_PS_NEAREST, NULL);
		double rad;
		if (this->doDegree2RadConversion) {
			rad = DEG2RAD((double)degree[0]);
		}
		else {
			rad = degree[0];
		}
		this->cosine = cos(rad);
		this->sine = sin(rad);
		
		isDegreeSet = true;
	}
}


void RotateOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	ensureDegree();
	const float dy = y - this->centerY;
	const float dx = x - this->centerX;
	const float nx = this->centerX + (this->cosine * dx + this->sine * dy);
	const float ny = this->centerY + (-this->sine * dx + this->cosine * dy);
	this->imageSocket->read(color, nx, ny, sampler, inputBuffers);
}

bool RotateOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	ensureDegree();
	rcti newInput;
	
	const float dxmin = input->xmin - this->centerX;
	const float dymin = input->ymin - this->centerY;
	const float dxmax = input->xmax - this->centerX;
	const float dymax = input->ymax - this->centerY;
	
	const float x1 = this->centerX + (this->cosine * dxmin + this->sine * dymin);
	const float x2 = this->centerX + (this->cosine * dxmax + this->sine * dymin);
	const float x3 = this->centerX + (this->cosine * dxmin + this->sine * dymax);
	const float x4 = this->centerX + (this->cosine * dxmax + this->sine * dymax);
	const float y1 = this->centerY + (-this->sine * dxmin + this->cosine * dymin);
	const float y2 = this->centerY + (-this->sine * dxmax + this->cosine * dymin);
	const float y3 = this->centerY + (-this->sine * dxmin + this->cosine * dymax);
	const float y4 = this->centerY + (-this->sine * dxmax + this->cosine * dymax);
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
