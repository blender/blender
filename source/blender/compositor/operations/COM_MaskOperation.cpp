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

#include "COM_MaskOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_scene_types.h"

extern "C" {
	#include "BKE_mask.h"
}

MaskOperation::MaskOperation(): NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->mask = NULL;
	this->maskWidth = 0;
	this->maskHeight = 0;
	this->framenumber = 0;
	this->rasterizedMask = NULL;
	setComplex(true);
}

void MaskOperation::initExecution()
{
	initMutex();
	this->rasterizedMask = NULL;
}

void MaskOperation::deinitExecution()
{
	if (this->rasterizedMask) {
		MEM_freeN(rasterizedMask);
		this->rasterizedMask = NULL;
	}
}

void *MaskOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->rasterizedMask)
		return this->rasterizedMask;

	if (!this->mask)
		return NULL;

	BLI_mutex_lock(getMutex());
	if (this->rasterizedMask == NULL) {
		int width = this->getWidth();
		int height = this->getHeight();
		float *buffer;

		buffer = (float *)MEM_callocN(sizeof(float) * width * height, "rasterized mask");
		BKE_mask_rasterize(mask, width, height, buffer);

		this->rasterizedMask = buffer;
	}
	BLI_mutex_unlock(getMutex());

	return this->rasterizedMask;
}

void MaskOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	if (maskWidth == 0 || maskHeight == 0) {
		NodeOperation::determineResolution(resolution, preferredResolution);
	}
	else {
		unsigned int nr[2];

		nr[0] = maskWidth;
		nr[1] = maskHeight;

		NodeOperation::determineResolution(resolution, nr);

		resolution[0] = maskWidth;
		resolution[1] = maskHeight;
	}
}

void MaskOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	if (!data) {
		color[0] = 0;
		color[1] = 0;
		color[2] = 0;
		color[3] = 1.0f;
	}
	else {
		float *buffer = (float*) data;
		int index = (y * this->getWidth() + x);

		color[0] = buffer[index];
		color[1] = buffer[index];
		color[2] = buffer[index];
		color[3] = 1.0f;
	}
}


