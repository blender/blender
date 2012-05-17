/*
 * Copyright 2012, Blender Foundation.
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
 *		Dalai Felinto
 */

#include "COM_ConvertKeyToPremulOperation.h"
#include "BLI_math.h"

ConvertKeyToPremulOperation::ConvertKeyToPremulOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->inputColor = NULL;
}

void ConvertKeyToPremulOperation::initExecution()
{
	this->inputColor = getInputSocketReader(0);
}

void ConvertKeyToPremulOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputValue[4];
	float alpha;

	this->inputColor->read(inputValue, x, y, sampler, inputBuffers);
	alpha = inputValue[3];

	outputValue[0] = inputValue[0] * alpha;
	outputValue[1] = inputValue[1] * alpha;
	outputValue[2] = inputValue[2] * alpha;

	/* never touches the alpha */
	outputValue[3] = alpha;
}

void ConvertKeyToPremulOperation::deinitExecution()
{
	this->inputColor = NULL;
}
