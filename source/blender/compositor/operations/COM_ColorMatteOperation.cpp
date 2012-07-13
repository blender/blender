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

#include "COM_ColorMatteOperation.h"
#include "BLI_math.h"

ColorMatteOperation::ColorMatteOperation() : NodeOperation()
{
	addInputSocket(COM_DT_COLOR);
	addInputSocket(COM_DT_COLOR);
	addOutputSocket(COM_DT_VALUE);

	this->m_inputImageProgram = NULL;
	this->m_inputKeyProgram = NULL;
}

void ColorMatteOperation::initExecution()
{
	this->m_inputImageProgram = this->getInputSocketReader(0);
	this->m_inputKeyProgram = this->getInputSocketReader(1);
}

void ColorMatteOperation::deinitExecution()
{
	this->m_inputImageProgram = NULL;
	this->m_inputKeyProgram = NULL;
}

void ColorMatteOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inColor[4];
	float inKey[4];

	const float hue = this->m_settings->t1;
	const float sat = this->m_settings->t2;
	const float val = this->m_settings->t3;

	float h_wrap;

	this->m_inputImageProgram->read(inColor, x, y, sampler);
	this->m_inputKeyProgram->read(inKey, x, y, sampler);


	/* store matte(alpha) value in [0] to go with
	 * COM_SetAlphaOperation and the Value output
	 */

	if (
	    /* do hue last because it needs to wrap, and does some more checks  */

	    /* sat */ (fabsf(inColor[1] - inKey[1]) < sat) &&
	    /* val */ (fabsf(inColor[2] - inKey[2]) < val) &&

	    /* multiply by 2 because it wraps on both sides of the hue,
	     * otherwise 0.5 would key all hue's */

	    /* hue */ ((h_wrap = 2.f * fabsf(inColor[0] - inKey[0])) < hue || (2.f - h_wrap) < hue)
	    )
	{
		outputValue[0] = 0.0f; /*make transparent*/
	}

	else { /*pixel is outside key color */
		outputValue[0] = inColor[3]; /* make pixel just as transparent as it was before */
	}
}

