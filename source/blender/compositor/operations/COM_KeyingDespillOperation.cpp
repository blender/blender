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

KeyingDespillOperation::KeyingDespillOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->m_despillFactor = 0.5f;
	this->m_colorBalance = 0.5f;

	this->m_pixelReader = NULL;
	this->m_screenReader = NULL;
}

void KeyingDespillOperation::initExecution()
{
	this->m_pixelReader = this->getInputSocketReader(0);
	this->m_screenReader = this->getInputSocketReader(1);
}

void KeyingDespillOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
	this->m_screenReader = NULL;
}

void KeyingDespillOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	float pixelColor[4];
	float screenColor[4];

	this->m_pixelReader->read(pixelColor, x, y, sampler);
	this->m_screenReader->read(screenColor, x, y, sampler);

	int screen_primary_channel = get_pixel_primary_channel(screenColor);
	int other_1 = (screen_primary_channel + 1) % 3;
	int other_2 = (screen_primary_channel + 2) % 3;

	int min_channel = MIN2(other_1, other_2);
	int max_channel = MAX2(other_1, other_2);

	float average_value, amount;

	average_value = this->m_colorBalance * pixelColor[min_channel] + (1.0f - this->m_colorBalance) * pixelColor[max_channel];
	amount = (pixelColor[screen_primary_channel] - average_value);

	color[0] = pixelColor[0];
	color[1] = pixelColor[1];
	color[2] = pixelColor[2];
	color[3] = pixelColor[3];

	if (this->m_despillFactor * amount > 0) {
		color[screen_primary_channel] = pixelColor[screen_primary_channel] - this->m_despillFactor * amount;
	}
}
