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

#include "COM_ColorRampOperation.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_texture.h"
#ifdef __cplusplus
}
#endif

ColorRampOperation::ColorRampOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);

	this->m_inputProgram = NULL;
	this->m_colorBand = NULL;
}
void ColorRampOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
}

void ColorRampOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	float values[4];

	this->m_inputProgram->read(values, x, y, sampler);
	do_colorband(this->m_colorBand, values[0], color);
}

void ColorRampOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
}
