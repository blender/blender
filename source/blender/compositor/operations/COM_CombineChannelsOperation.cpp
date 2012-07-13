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

bool CombineChannelsOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) 
{
	rcti tempOutput;
	bool first = true;
	for (int i = 0 ; i < 4 ; i ++) {
		NodeOperation * inputOperation = this->getInputOperation(i);
		if (inputOperation->determineDependingAreaOfInterest(input, readOperation, &tempOutput)) {
			if (first) {
				output->xmin = tempOutput.xmin;
				output->ymin = tempOutput.ymin;
				output->xmax = tempOutput.xmax;
				output->ymax = tempOutput.ymax;
				first = false;
			} else {
				output->xmin = MIN2(output->xmin, tempOutput.xmin);
				output->ymin = MIN2(output->ymin, tempOutput.ymin);
				output->xmax = MAX2(output->xmax, tempOutput.xmax);
				output->ymax = MAX2(output->ymax, tempOutput.ymax);
			}
		}
	}
	return !first;
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


void CombineChannelsOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	float input[4];
	/// @todo: remove if statements
	if (this->m_inputChannel1Operation) {
		this->m_inputChannel1Operation->read(input, x, y, sampler);
		color[0] = input[0];
	}
	if (this->m_inputChannel2Operation) {
		this->m_inputChannel2Operation->read(input, x, y, sampler);
		color[1] = input[0];
	}
	if (this->m_inputChannel3Operation) {
		this->m_inputChannel3Operation->read(input, x, y, sampler);
		color[2] = input[0];
	}
	if (this->m_inputChannel4Operation) {
		this->m_inputChannel4Operation->read(input, x, y, sampler);
		color[3] = input[0];
	}
}
