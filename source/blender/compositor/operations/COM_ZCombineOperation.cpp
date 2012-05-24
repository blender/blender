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

#include "COM_ZCombineOperation.h"
#include "BLI_utildefines.h"

ZCombineOperation::ZCombineOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);

	this->image1Reader = NULL;
	this->depth1Reader = NULL;
	this->image2Reader = NULL;
	this->depth2Reader = NULL;

}

void ZCombineOperation::initExecution()
{
	this->image1Reader = this->getInputSocketReader(0);
	this->depth1Reader = this->getInputSocketReader(1);
	this->image2Reader = this->getInputSocketReader(2);
	this->depth2Reader = this->getInputSocketReader(3);
}

void ZCombineOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float depth1[4];
	float depth2[4];

	this->depth1Reader->read(depth1, x, y, sampler, inputBuffers);
	this->depth2Reader->read(depth2, x, y, sampler, inputBuffers);
	if (depth1[0]<depth2[0]) {
		this->image1Reader->read(color, x, y, sampler, inputBuffers);
	}
	else {
		this->image2Reader->read(color, x, y, sampler, inputBuffers);
	}
}
void ZCombineAlphaOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float depth1[4];
	float depth2[4];
	float color1[4];
	float color2[4];

	this->depth1Reader->read(depth1, x, y, sampler, inputBuffers);
	this->depth2Reader->read(depth2, x, y, sampler, inputBuffers);
	if (depth1[0]<depth2[0]) {
		this->image1Reader->read(color1, x, y, sampler, inputBuffers);
		this->image2Reader->read(color2, x, y, sampler, inputBuffers);
	}
	else {
		this->image1Reader->read(color2, x, y, sampler, inputBuffers);
		this->image2Reader->read(color1, x, y, sampler, inputBuffers);
	}
	float fac = color1[3];
	float ifac = 1.0f-fac;
	color[0] = color1[0]+ifac*color2[0];
	color[1] = color1[1]+ifac*color2[1];
	color[2] = color1[2]+ifac*color2[2];
	color[3] = MAX2(color1[3], color2[3]);
}

void ZCombineOperation::deinitExecution()
{
	this->image1Reader = NULL;
	this->depth1Reader = NULL;
	this->image2Reader = NULL;
	this->depth2Reader = NULL;
}
