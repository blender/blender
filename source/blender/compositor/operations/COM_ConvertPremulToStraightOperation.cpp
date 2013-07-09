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

#include "COM_ConvertPremulToStraightOperation.h"
#include "BLI_math.h"

ConvertPremulToStraightOperation::ConvertPremulToStraightOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->m_inputColor = NULL;
}

void ConvertPremulToStraightOperation::initExecution()
{
	this->m_inputColor = getInputSocketReader(0);
}

void ConvertPremulToStraightOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue[4];
	float alpha;

	this->m_inputColor->read(inputValue, x, y, sampler);
	alpha = inputValue[3];

	if (fabsf(alpha) < 1e-5f) {
		zero_v3(output);
	}
	else {
		mul_v3_v3fl(output, inputValue, 1.0f / alpha);
	}

	/* never touches the alpha */
	output[3] = alpha;
}

void ConvertPremulToStraightOperation::deinitExecution()
{
	this->m_inputColor = NULL;
}
