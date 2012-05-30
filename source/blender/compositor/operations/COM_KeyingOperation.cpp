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

#include "COM_KeyingOperation.h"

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

static float get_pixel_saturation(float *pixel, float screen_balance)
{
	float min = MIN3(pixel[0], pixel[1], pixel[2]);
	float max = MAX3(pixel[0], pixel[1], pixel[2]);
	float mid = pixel[0] + pixel[1] + pixel[2] - min - max;
	float val = (1.0f - screen_balance) * min + screen_balance * mid;

	return (max - val) * (1.0f - val) * (1.0f - val);
}

KeyingOperation::KeyingOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_VALUE);

	this->screenBalance = 0.5f;
	this->clipBlack = 0.0f;
	this->clipWhite = 1.0f;

	this->pixelReader = NULL;
	this->screenReader = NULL;
}

void KeyingOperation::initExecution()
{
	this->pixelReader = this->getInputSocketReader(0);
	this->screenReader = this->getInputSocketReader(1);
}

void KeyingOperation::deinitExecution()
{
	this->pixelReader = NULL;
	this->screenReader = NULL;
}

void KeyingOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float pixelColor[4];
	float screenColor[4];

	this->pixelReader->read(pixelColor, x, y, sampler, inputBuffers);
	this->screenReader->read(screenColor, x, y, sampler, inputBuffers);

	float saturation = get_pixel_saturation(pixelColor, this->screenBalance);
	float screen_saturation = get_pixel_saturation(screenColor, this->screenBalance);
	int primary_channel = get_pixel_primary_channel(pixelColor);
	int screen_primary_channel = get_pixel_primary_channel(screenColor);

	if (primary_channel != screen_primary_channel) {
		/* different main channel means pixel is on foreground */
		color[0] = 1.0f;
	}
        else if (saturation >= screen_saturation) {
		/* saturation of main channel is more than screen, definitely a background */
		color[0] = 0.0f;
	}
	else {
		float distance;

		distance = 1.0f - saturation / screen_saturation;

		color[0] = distance * distance;

		if (color[0] < this->clipBlack)
			color[0] = 0.0f;
		else if (color[0] >= this->clipWhite)
			color[0] = 1.0f;
		else
			color[0] = (color[0] - this->clipBlack) / (this->clipWhite - this->clipBlack);
	}
}
