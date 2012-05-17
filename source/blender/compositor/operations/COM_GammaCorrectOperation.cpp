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

#include "COM_GammaCorrectOperation.h"
#include "BLI_math.h"

GammaCorrectOperation::GammaCorrectOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputProgram = NULL;
}
void GammaCorrectOperation::initExecution()
{
	this->inputProgram = this->getInputSocketReader(0);
}

void GammaCorrectOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor[4];
	this->inputProgram->read(inputColor, x, y, sampler, inputBuffers);
	if (inputColor[3] > 0.0f) {
		inputColor[0] /= inputColor[3];
		inputColor[1] /= inputColor[3];
		inputColor[2] /= inputColor[3];
	}

	/* check for negative to avoid nan's */
	color[0] = inputColor[0]>0.0f?inputColor[0]*inputColor[0] :0.0f;
	color[1] = inputColor[1]>0.0f?inputColor[1]*inputColor[1] :0.0f;
	color[2] = inputColor[2]>0.0f?inputColor[2]*inputColor[2] :0.0f;

	inputColor[0] *= inputColor[3];
	inputColor[1] *= inputColor[3];
	inputColor[2] *= inputColor[3];

	color[0] = inputColor[0];
	color[1] = inputColor[1];
	color[2] = inputColor[2];
	color[3] = inputColor[3];
}

void GammaCorrectOperation::deinitExecution()
{
	this->inputProgram = NULL;
}

GammaUncorrectOperation::GammaUncorrectOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputProgram = NULL;
}
void GammaUncorrectOperation::initExecution()
{
	this->inputProgram = this->getInputSocketReader(0);
}

void GammaUncorrectOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor[4];
	this->inputProgram->read(inputColor, x, y, sampler, inputBuffers);

	if (inputColor[3] > 0.0f) {
		inputColor[0] /= inputColor[3];
		inputColor[1] /= inputColor[3];
		inputColor[2] /= inputColor[3];
	}

	color[0] = inputColor[0]>0.0f?sqrtf(inputColor[0]) :0.0f;
	color[1] = inputColor[1]>0.0f?sqrtf(inputColor[1]) :0.0f;
	color[2] = inputColor[2]>0.0f?sqrtf(inputColor[2]) :0.0f;

	inputColor[0] *= inputColor[3];
	inputColor[1] *= inputColor[3];
	inputColor[2] *= inputColor[3];

	color[0] = inputColor[0];
	color[1] = inputColor[1];
	color[2] = inputColor[2];
	color[3] = inputColor[3];
}

void GammaUncorrectOperation::deinitExecution()
{
	this->inputProgram = NULL;
}
