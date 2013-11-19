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

#include "COM_ChangeHSVOperation.h"

ChangeHSVOperation::ChangeHSVOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputOperation = NULL;
}

void ChangeHSVOperation::initExecution()
{
	this->m_inputOperation = getInputSocketReader(0);
}

void ChangeHSVOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}

void ChangeHSVOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	
	this->m_inputOperation->readSampled(inputColor1, x, y, sampler);
	
	output[0] = inputColor1[0] + (this->m_hue - 0.5f);
	if      (output[0] > 1.0f) output[0] -= 1.0f;
	else if (output[0] < 0.0f) output[0] += 1.0f;
	output[1] = inputColor1[1] * this->m_saturation;
	output[2] = inputColor1[2] * this->m_value;
	output[3] = inputColor1[3];
}

