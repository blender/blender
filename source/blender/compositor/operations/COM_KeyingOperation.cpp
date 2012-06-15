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

static int get_pixel_primary_channel(float pixelColor[4])
{
	float max_value = MAX3(pixelColor[0], pixelColor[1], pixelColor[2]);

	if (max_value == pixelColor[0])
		return 0;
	else if (max_value == pixelColor[1])
		return 1;

	return 2;
}

static float get_pixel_saturation(float pixelColor[4], float screen_balance, int primary_channel)
{
	int other_1 = (primary_channel + 1) % 3;
	int other_2 = (primary_channel + 2) % 3;

	float min = MIN2(pixelColor[other_1], pixelColor[other_2]);
	float max = MAX2(pixelColor[other_1], pixelColor[other_2]);
	float val = screen_balance * min + (1.0f - screen_balance) * max;

	return (pixelColor[primary_channel] - val) * fabsf(1.0f - val);
}

KeyingOperation::KeyingOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);

	this->screenBalance = 0.5f;

	this->pixelReader = NULL;
	this->screenReader = NULL;
	this->garbageReader = NULL;
	this->coreReader = NULL;
}

void KeyingOperation::initExecution()
{
	this->pixelReader = this->getInputSocketReader(0);
	this->screenReader = this->getInputSocketReader(1);
	this->garbageReader = this->getInputSocketReader(2);
	this->coreReader = this->getInputSocketReader(3);
}

void KeyingOperation::deinitExecution()
{
	this->pixelReader = NULL;
	this->screenReader = NULL;
	this->garbageReader = NULL;
	this->coreReader = NULL;
}

void KeyingOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float pixelColor[4];
	float screenColor[4];
	float garbageValue[4];
	float coreValue[4];

	this->pixelReader->read(pixelColor, x, y, sampler, inputBuffers);
	this->screenReader->read(screenColor, x, y, sampler, inputBuffers);
	this->garbageReader->read(garbageValue, x, y, sampler, inputBuffers);
	this->coreReader->read(coreValue, x, y, sampler, inputBuffers);

	int primary_channel = get_pixel_primary_channel(screenColor);

	float saturation = get_pixel_saturation(pixelColor, this->screenBalance, primary_channel);
	float screen_saturation = get_pixel_saturation(screenColor, this->screenBalance, primary_channel);

	if (saturation < 0) {
		color[0] = 1.0f;
	}
	else if (saturation >= screen_saturation) {
		color[0] = 0.0f;
	}
	else {
		float distance = 1.0f - saturation / screen_saturation;

		color[0] = distance;
	}

	color[0] *= (1.0f - garbageValue[0]);

	color[0] = MAX2(color[0], coreValue[0]);
}

bool KeyingOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmin = 0;
	newInput.ymin = 0;
	newInput.xmax = this->getWidth();
	newInput.ymax = this->getHeight();

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
