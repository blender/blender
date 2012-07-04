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

void *KeyingBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect, memoryBuffers);

	return buffer;
}

void KeyingBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();

	int bufferWidth = inputBuffer->getWidth();
	int bufferHeight = inputBuffer->getHeight();

	int i, count = 0;

	float average = 0.0f;

	if (this->m_axis == 0) {
		for (i = -this->m_size + 1; i < this->m_size; i++) {
			int cx = x + i;

			if (cx >= 0 && cx < bufferWidth) {
				int bufferIndex = (y * bufferWidth + cx) * 4;

				average += buffer[bufferIndex];
				count++;
			}
		}
	}
	else {
		for (i = -this->m_size + 1; i < this->m_size; i++) {
			int cy = y + i;

			if (cy >= 0 && cy < bufferHeight) {
				int bufferIndex = (cy * bufferWidth + x) * 4;

				average += buffer[bufferIndex];
				count++;
			}
		}
	}

	average /= (float) count;

	color[0] = average;
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
