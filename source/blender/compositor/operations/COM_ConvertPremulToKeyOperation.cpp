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

#include "COM_ConvertPremulToKeyOperation.h"
#include "BLI_math.h"

ConvertPremulToKeyOperation::ConvertPremulToKeyOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->m_inputColor = NULL;
}

void ConvertPremulToKeyOperation::initExecution()
{
	this->m_inputColor = getInputSocketReader(0);
}

void ConvertPremulToKeyOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputValue[4];
	float alpha;

	this->m_inputColor->read(inputValue, x, y, sampler, inputBuffers);
	alpha = inputValue[3];

	if (fabsf(alpha) < 1e-5f) {
		zero_v3(outputValue);
	}
	else {
		mul_v3_v3fl(outputValue, inputValue, 1.0f / alpha);
	}

	/* never touches the alpha */
	outputValue[3] = alpha;
}

void ConvertPremulToKeyOperation::deinitExecution()
{
	this->m_inputColor = NULL;
}
