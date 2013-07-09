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

#include "COM_GlareBaseOperation.h"
#include "BLI_math.h"

GlareBaseOperation::GlareBaseOperation() : SingleThreadedNodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_settings = NULL;
}
void GlareBaseOperation::initExecution()
{
	SingleThreadedNodeOperation::initExecution();
	this->m_inputProgram = getInputSocketReader(0);
}

void GlareBaseOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
	SingleThreadedNodeOperation::deinitExecution();
}

MemoryBuffer *GlareBaseOperation::createMemoryBuffer(rcti *rect2)
{
	MemoryBuffer *tile = (MemoryBuffer *)this->m_inputProgram->initializeTileData(rect2);
	rcti rect;
	rect.xmin = 0;
	rect.ymin = 0;
	rect.xmax = getWidth();
	rect.ymax = getHeight();
	MemoryBuffer *result = new MemoryBuffer(NULL, &rect);
	float *data = result->getBuffer();
	this->generateGlare(data, tile, this->m_settings);
	return result;
}

bool GlareBaseOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (isCached()) {
		return false;
	}
	else {
		rcti newInput;
		newInput.xmax = this->getWidth();
		newInput.xmin = 0;
		newInput.ymax = this->getHeight();
		newInput.ymin = 0;
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}
