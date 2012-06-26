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

#include "COM_ConvertRGBToYCCOperation.h"
#include "BLI_math_color.h"

ConvertRGBToYCCOperation::ConvertRGBToYCCOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputOperation = NULL;
}

void ConvertRGBToYCCOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
}

void ConvertRGBToYCCOperation::setMode(int mode)
{
	switch (mode) {
		case 1:
			this->m_mode = BLI_YCC_ITU_BT709;
			break;
		case 2:
			this->m_mode = BLI_YCC_JFIF_0_255;
			break;
		case 0:
		default:
			this->m_mode = BLI_YCC_ITU_BT601;
			break;
	}
}

void ConvertRGBToYCCOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor[4];
	float color[3];

	this->m_inputOperation->read(inputColor, x, y, sampler, inputBuffers);
	rgb_to_ycc(inputColor[0], inputColor[1], inputColor[2], &color[0], &color[1], &color[2], this->m_mode);

	/* divided by 255 to normalize for viewing in */
	/* R,G,B --> Y,Cb,Cr */
	mul_v3_v3fl(outputValue, color, 1.0f / 255.0f);
	outputValue[3] = inputColor[3];
}

void ConvertRGBToYCCOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}
