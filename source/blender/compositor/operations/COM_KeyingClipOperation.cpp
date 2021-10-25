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

void *KeyingClipOperation::initializeTileData(rcti *rect)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect);

	return buffer;
}

void KeyingClipOperation::executePixel(float output[4], int x, int y, void *data)
{
	const int delta = this->m_kernelRadius;
	const float tolerance = this->m_kernelTolerance;

	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();

	int bufferWidth = inputBuffer->getWidth();
	int bufferHeight = inputBuffer->getHeight();

	float value = buffer[(y * bufferWidth + x)];

	bool ok = false;
	int start_x = max_ff(0, x - delta + 1),
	    start_y = max_ff(0, y - delta + 1),
	    end_x = min_ff(x + delta - 1, bufferWidth - 1),
	    end_y = min_ff(y + delta - 1, bufferHeight - 1);

	int count = 0, totalCount = (end_x - start_x + 1) * (end_y - start_y + 1) - 1;
	int thresholdCount = ceil((float) totalCount * 0.9f);

	if (delta == 0) {
		ok = true;
	}

	for (int cx = start_x; ok == false && cx <= end_x; ++cx) {
		for (int cy = start_y; ok == false && cy <= end_y; ++cy) {
			if (UNLIKELY(cx == x && cy == y)) {
				continue;
			}

			int bufferIndex = (cy * bufferWidth + cx);
			float currentValue = buffer[bufferIndex];

			if (fabsf(currentValue - value) < tolerance) {
				count++;
				if (count >= thresholdCount) {
					ok = true;
				}
			}
		}
	}

	if (this->m_isEdgeMatte) {
		if (ok)
			output[0] = 0.0f;
		else
			output[0] = 1.0f;
	}
	else {
		output[0] = value;

		if (ok) {
			if (output[0] < this->m_clipBlack)
				output[0] = 0.0f;
			else if (output[0] >= this->m_clipWhite)
				output[0] = 1.0f;
			else
				output[0] = (output[0] - this->m_clipBlack) / (this->m_clipWhite - this->m_clipBlack);
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
