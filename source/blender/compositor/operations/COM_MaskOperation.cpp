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
	#include "../../../../intern/raskter/raskter.h"
}

MaskOperation::MaskOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
	this->m_mask = NULL;
	this->m_maskWidth = 0;
	this->m_maskHeight = 0;
	this->m_framenumber = 0;
	this->m_rasterizedMask = NULL;
	setComplex(true);
}

void MaskOperation::initExecution()
{
	initMutex();
	this->m_rasterizedMask = NULL;
}

void MaskOperation::deinitExecution()
{
	if (this->m_rasterizedMask) {
		MEM_freeN(this->m_rasterizedMask);
		this->m_rasterizedMask = NULL;
	}
}

void *MaskOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->m_rasterizedMask)
		return this->m_rasterizedMask;

	if (!this->m_mask)
		return NULL;

	lockMutex();
	if (this->m_rasterizedMask == NULL) {
		int width = this->getWidth();
		int height = this->getHeight();
		float *buffer;

		buffer = (float *)MEM_callocN(sizeof(float) * width * height, "rasterized mask");
		BKE_mask_rasterize(this->m_mask, width, height, buffer, TRUE, this->m_do_smooth, this->m_do_feather);
		if (this->m_do_smooth) {
			PLX_antialias_buffer(buffer, width, height);
		}

		this->m_rasterizedMask = buffer;
	}
	unlockMutex();
	return this->m_rasterizedMask;
}

void MaskOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	if (this->m_maskWidth == 0 || this->m_maskHeight == 0) {
		NodeOperation::determineResolution(resolution, preferredResolution);
	}
	else {
		unsigned int nr[2];

		nr[0] = this->m_maskWidth;
		nr[1] = this->m_maskHeight;

		NodeOperation::determineResolution(resolution, nr);

		resolution[0] = this->m_maskWidth;
		resolution[1] = this->m_maskHeight;
	}
}

void MaskOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	if (!data) {
		color[0] = 0.0f;
	}
	else {
		float *buffer = (float *) data;
		int index = (y * this->getWidth() + x);

		color[0] = buffer[index];
	}
}
