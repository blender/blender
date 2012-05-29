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

#include "COM_KeyingDespillOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

static int get_pixel_primary_channel(float *pixel)
{
	float max_value = MAX3(pixel[0], pixel[1], pixel[2]);

	if (max_value == pixel[0])
		return 0;
	else if (max_value == pixel[1])
		return 1;

	return 2;
}

KeyingDespillOperation::KeyingDespillOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->despillFactor = 0.5f;

	this->pixelReader = NULL;
	this->screenReader = NULL;
}

void KeyingDespillOperation::initExecution()
{
	this->pixelReader = this->getInputSocketReader(0);
	this->screenReader = this->getInputSocketReader(1);
}

void KeyingDespillOperation::deinitExecution()
{
	this->pixelReader = NULL;
	this->screenReader = NULL;
}

void KeyingDespillOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float pixelColor[4];
	float screenColor[4];

	this->pixelReader->read(pixelColor, x, y, sampler, inputBuffers);
	this->screenReader->read(screenColor, x, y, sampler, inputBuffers);

	int screen_primary_channel = get_pixel_primary_channel(screenColor);
	float average_value, amount;

	average_value = (pixelColor[0] + pixelColor[1] + pixelColor[2] - pixelColor[screen_primary_channel]) / 2.0f;
	amount = pixelColor[screen_primary_channel] - average_value;

	color[0] = pixelColor[0];
	color[1] = pixelColor[1];
	color[2] = pixelColor[2];
	color[3] = pixelColor[3];
	
	if (this->despillFactor * amount > 0) {
		color[screen_primary_channel] = pixelColor[screen_primary_channel] - this->despillFactor * amount;
	}
}
