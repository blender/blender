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

#include "COM_KeyingClipOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

KeyingClipOperation::KeyingClipOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);

	this->m_kernelRadius = 3;
	this->m_kernelTolerance = 0.1f;

	this->m_clipBlack = 0.0f;
	this->m_clipWhite = 1.0f;

	this->m_isEdgeMatte = false;

	this->setComplex(true);
}

void *KeyingClipOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect, memoryBuffers);

	return buffer;
}

void KeyingClipOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	const int delta = this->m_kernelRadius;
	const float tolerance = this->m_kernelTolerance;

	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();

	int bufferWidth = inputBuffer->getWidth();
	int bufferHeight = inputBuffer->getHeight();

	int i, j, count = 0, totalCount = 0;

	float value = buffer[(y * bufferWidth + x) * 4];

	bool ok = false;

	for (i = -delta + 1; i < delta; i++) {
		for (j = -delta + 1; j < delta; j++) {
			int cx = x + j, cy = y + i;

			if (i == 0 && j == 0)
				continue;

			if (cx >= 0 && cx < bufferWidth && cy >= 0 && cy < bufferHeight) {
				int bufferIndex = (cy * bufferWidth + cx) * 4;
				float currentValue = buffer[bufferIndex];

				if (fabsf(currentValue - value) < tolerance) {
					count++;
				}

				totalCount++;
			}
		}
	}

	ok = count >= (float) totalCount * 0.9f;

	if (this->m_isEdgeMatte) {
		if (ok)
			color[0] = 0.0f;
		else
			color[0] = 1.0f;
	}
	else {
		color[0] = value;

		if (ok) {
			if (color[0] < this->m_clipBlack)
				color[0] = 0.0f;
			else if (color[0] >= this->m_clipWhite)
				color[0] = 1.0f;
			else
				color[0] = (color[0] - this->m_clipBlack) / (this->m_clipWhite - this->m_clipBlack);
		}
	}
}

bool KeyingClipOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmin = input->xmin - this->m_kernelRadius;
	newInput.ymin = input->ymin - this->m_kernelRadius;
	newInput.xmax = input->xmax + this->m_kernelRadius;
	newInput.ymax = input->ymax + this->m_kernelRadius;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
