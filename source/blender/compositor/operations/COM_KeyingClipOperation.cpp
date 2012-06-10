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

KeyingClipOperation::KeyingClipOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);

	this->clipBlack = 0.0f;
	this->clipWhite = 1.0f;

	this->setComplex(true);
}

void *KeyingClipOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = getInputOperation(0)->initializeTileData(rect, memoryBuffers);

	return buffer;
}

void KeyingClipOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	const int delta = 3;

	MemoryBuffer *inputBuffer = (MemoryBuffer*)data;
	float *buffer = inputBuffer->getBuffer();

	int bufferWidth = inputBuffer->getWidth();
	int bufferHeight = inputBuffer->getHeight();

	int count_black = 0, count_white = 0;
	int i, j;

	int srcIndex = (y * bufferWidth + x) * 4;

	for (i = -delta + 1; i < delta; i++) {
		for (j = -delta + 1; j < delta; j++) {
			int cx = x + j, cy = y + i;

			if (i == 0 && j == 0)
				continue;

			if (cx >= 0 && cx < bufferWidth && cy >= 0 && cy < bufferHeight) {
				int bufferIndex = (cy * bufferWidth + cx) * 4;

				if (buffer[bufferIndex] < 0.4f)
					count_black++;
				else if (buffer[bufferIndex] > 0.6f)
					count_white++;
			}
		}
	}

	color[0] = buffer[srcIndex];

	if (count_black >= 22 || count_white >= 22) {
		if (color[0] < this->clipBlack)
			color[0] = 0.0f;
		else if (color[0] >= this->clipWhite)
			color[0] = 1.0f;
		else
			color[0] = (color[0] - this->clipBlack) / (this->clipWhite - this->clipBlack);
	}
}
