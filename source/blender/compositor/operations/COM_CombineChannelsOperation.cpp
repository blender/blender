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

#include "COM_CombineChannelsOperation.h"
#include "BLI_utildefines.h"

CombineChannelsOperation::CombineChannelsOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputChannel1Operation = NULL;
	this->m_inputChannel2Operation = NULL;
	this->m_inputChannel3Operation = NULL;
	this->m_inputChannel4Operation = NULL;
}

void CombineChannelsOperation::initExecution()
{
	this->m_inputChannel1Operation = this->getInputSocketReader(0);
	this->m_inputChannel2Operation = this->getInputSocketReader(1);
	this->m_inputChannel3Operation = this->getInputSocketReader(2);
	this->m_inputChannel4Operation = this->getInputSocketReader(3);
}

void CombineChannelsOperation::deinitExecution()
{
	this->m_inputChannel1Operation = NULL;
	this->m_inputChannel2Operation = NULL;
	this->m_inputChannel3Operation = NULL;
	this->m_inputChannel4Operation = NULL;
}


void CombineChannelsOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	float input[4];
	if (this->m_inputChannel1Operation) {
		this->m_inputChannel1Operation->read(input, x, y, sampler);
		output[0] = input[0];
	}
	if (this->m_inputChannel2Operation) {
		this->m_inputChannel2Operation->read(input, x, y, sampler);
		output[1] = input[0];
	}
	if (this->m_inputChannel3Operation) {
		this->m_inputChannel3Operation->read(input, x, y, sampler);
		output[2] = input[0];
	}
	if (this->m_inputChannel4Operation) {
		this->m_inputChannel4Operation->read(input, x, y, sampler);
		output[3] = input[0];
	}
}
