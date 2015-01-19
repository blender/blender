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
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */

#include "COM_KeyingBlurOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

KeyingBlurOperation::KeyingBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);

	this->m_size = 0;
	this->m_axis = BLUR_AXIS_X;

	this->setComplex(true);
}

void *KeyingBlurOperation::initializeTileData(rcti *rect)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect);

	return buffer;
}

void KeyingBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	const int bufferWidth = inputBuffer->getWidth();
	float *buffer = inputBuffer->getBuffer();
	int count = 0;
	float average = 0.0f;

	if (this->m_axis == 0) {
		const int start = max(0, x - this->m_size + 1),
		          end = min(bufferWidth, x + this->m_size);
		for (int cx = start; cx < end; ++cx) {
			int bufferIndex = (y * bufferWidth + cx);
			average += buffer[bufferIndex];
			count++;
		}
	}
	else {
		const int start = max(0, y - this->m_size + 1),
		          end = min(inputBuffer->getHeight(), y + this->m_size);
		for (int cy = start; cy < end; ++cy) {
			int bufferIndex = (cy * bufferWidth + x);
			average += buffer[bufferIndex];
			count++;
		}
	}

	average /= (float) count;

	output[0] = average;
}

bool KeyingBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	if (this->m_axis == BLUR_AXIS_X) {
		newInput.xmin = input->xmin - this->m_size;
		newInput.ymin = input->ymin;
		newInput.xmax = input->xmax + this->m_size;
		newInput.ymax = input->ymax;
	}
	else {
		newInput.xmin = input->xmin;
		newInput.ymin = input->ymin - this->m_size;
		newInput.xmax = input->xmax;
		newInput.ymax = input->ymax + this->m_size;
	}

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
