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

	this->size = 0.0f;

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

	int i, j, count = 0;

	float average = 0.0f;

	for (i = -this->size + 1; i < this->size; i++) {
		for (j = -this->size + 1; j < this->size; j++) {
			int cx = x + j, cy = y + i;

			if (cx >= 0 && cx < bufferWidth && cy >= 0 && cy < bufferHeight) {
				int bufferIndex = (cy * bufferWidth + cx) * 4;

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

	newInput.xmin = input->xmin - this->size;
	newInput.ymin = input->ymin - this->size;
	newInput.xmax = input->xmax + this->size;
	newInput.ymax = input->ymax + this->size;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
