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
 *		Dalai Felinto
 */

#include "COM_LuminanceMatteOperation.h"
#include "BLI_math.h"

LuminanceMatteOperation::LuminanceMatteOperation() : NodeOperation()
{
	addInputSocket(COM_DT_COLOR);
	addOutputSocket(COM_DT_VALUE);

	this->m_inputImageProgram = NULL;
}

void LuminanceMatteOperation::initExecution()
{
	this->m_inputImageProgram = this->getInputSocketReader(0);
}

void LuminanceMatteOperation::deinitExecution()
{
	this->m_inputImageProgram = NULL;
}

void LuminanceMatteOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inColor[4];

	const float high = this->m_settings->t1;
	const float low = this->m_settings->t2;

	float alpha;

	this->m_inputImageProgram->readSampled(inColor, x, y, sampler);
	
	/* one line thread-friend algorithm:
	 * output[0] = max(inputValue[3], min(high, max(low, ((inColor[0] - low) / (high - low))));
	 */
		
	/* test range */
	if (inColor[0] > high) {
		alpha = 1.0f;
	}
	else if (inColor[0] < low) {
		alpha = 0.0f;
	}
	else { /*blend */
		alpha = (inColor[0] - low) / (high - low);
	}


	/* store matte(alpha) value in [0] to go with
	 * COM_SetAlphaOperation and the Value output
	 */

	/* don't make something that was more transparent less transparent */
	if (alpha < inColor[3]) {
		output[0] = alpha;
	}
	else {
		/* leave now it was before */
		output[0] = inColor[3];
	}
}

