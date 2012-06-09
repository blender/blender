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

	this->pixelReader = NULL;
}

void KeyingClipOperation::initExecution()
{
	this->pixelReader = this->getInputSocketReader(0);
}

void KeyingClipOperation::deinitExecution()
{
	this->pixelReader = NULL;
}

void KeyingClipOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	const int delta = 3;

	float pixelColor[4];
	int width = this->getWidth(), height = this->getHeight();
	int count_black = 0, count_white = 0;
	int i, j;

	this->pixelReader->read(pixelColor, x, y, sampler, inputBuffers);

	for (i = -delta + 1; i < delta; i++) {
		for (j = -delta + 1; j < delta; j++) {
			int cx = x + j, cy = y + i;

			if (i == 0 && j == 0)
				continue;

			if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
				float value[4];

				this->pixelReader->read(value, cx, cy, sampler, inputBuffers);

				if (value[0] < 0.4f)
					count_black++;
				else if (value[0] > 0.6f)
					count_white++;
			}
		}
	}

	color[0] = pixelColor[0];

	if (count_black >= 22 || count_white >= 22) {
		if (color[0] < this->clipBlack)
			color[0] = 0.0f;
		else if (color[0] >= this->clipWhite)
			color[0] = 1.0f;
		else
			color[0] = (color[0] - this->clipBlack) / (this->clipWhite - this->clipBlack);
	}
}
