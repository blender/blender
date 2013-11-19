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

#include "COM_ChannelMatteOperation.h"
#include "BLI_math.h"

ChannelMatteOperation::ChannelMatteOperation() : NodeOperation()
{
	addInputSocket(COM_DT_COLOR);
	addOutputSocket(COM_DT_VALUE);

	this->m_inputImageProgram = NULL;
}

void ChannelMatteOperation::initExecution()
{
	this->m_inputImageProgram = this->getInputSocketReader(0);

	this->m_limit_range = this->m_limit_max - this->m_limit_min;

	switch (this->m_limit_method) {
		/* SINGLE */
		case 0:
		{
			/* 123 / RGB / HSV / YUV / YCC */
			const int matte_channel = this->m_matte_channel - 1;
			const int limit_channel = this->m_limit_channel - 1;
			this->m_ids[0] = matte_channel;
			this->m_ids[1] = limit_channel;
			this->m_ids[2] = limit_channel;
			break;
		}
		/* MAX */
		case 1:
		{
			switch (this->m_matte_channel) {
				case 1:
				{
					this->m_ids[0] = 0;
					this->m_ids[1] = 1;
					this->m_ids[2] = 2;
					break;
				}
				case 2:
				{
					this->m_ids[0] = 1;
					this->m_ids[1] = 0;
					this->m_ids[2] = 2;
					break;
				}
				case 3:
				{
					this->m_ids[0] = 2;
					this->m_ids[1] = 0;
					this->m_ids[2] = 1;
					break;
				}
				default:
					break;
			}
			break;
		}
		default:
			break;
	}
}

void ChannelMatteOperation::deinitExecution()
{
	this->m_inputImageProgram = NULL;
}

void ChannelMatteOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inColor[4];
	float alpha;

	const float limit_max = this->m_limit_max;
	const float limit_min = this->m_limit_min;
	const float limit_range = this->m_limit_range;

	this->m_inputImageProgram->readSampled(inColor, x, y, sampler);

	/* matte operation */
	alpha = inColor[this->m_ids[0]] - max(inColor[this->m_ids[1]], inColor[this->m_ids[2]]);
		
	/* flip because 0.0 is transparent, not 1.0 */
	alpha = 1.0f - alpha;
	
	/* test range */
	if (alpha > limit_max) {
		alpha = inColor[3]; /*whatever it was prior */
	}
	else if (alpha < limit_min) {
		alpha = 0.f;
	}
	else { /*blend */
		alpha = (alpha - limit_min) / limit_range;
	}

	/* store matte(alpha) value in [0] to go with
	 * COM_SetAlphaOperation and the Value output
	 */
	
	/* don't make something that was more transparent less transparent */
	output[0] = min(alpha, inColor[3]);
}

