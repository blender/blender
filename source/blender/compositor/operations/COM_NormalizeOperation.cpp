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
 *		Dalai Felinto
 */

#include "COM_NormalizeOperation.h"

NormalizeOperation::NormalizeOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_imageReader = NULL;
	this->m_cachedInstance = NULL;
	this->setComplex(true);
}
void NormalizeOperation::initExecution()
{
	this->m_imageReader = this->getInputSocketReader(0);
	NodeOperation::initMutex();
}

void NormalizeOperation::executePixel(float *color, int x, int y, void *data)
{
	/* using generic two floats struct to store x: min  y: mult */
	NodeTwoFloats *minmult = (NodeTwoFloats *)data;

	float output[4];
	this->m_imageReader->read(output, x, y, NULL);

	color[0] = (output[0] - minmult->x) * minmult->y;
}

void NormalizeOperation::deinitExecution()
{
	this->m_imageReader = NULL;
	if (this->m_cachedInstance) {
		delete this->m_cachedInstance;
	}
	NodeOperation::deinitMutex();
}

bool NormalizeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti imageInput;
	if (this->m_cachedInstance) return false;
	
	NodeOperation *operation = getInputOperation(0);
	imageInput.xmax = operation->getWidth();
	imageInput.xmin = 0;
	imageInput.ymax = operation->getHeight();
	imageInput.ymin = 0;

	if (operation->determineDependingAreaOfInterest(&imageInput, readOperation, output) ) {
		return true;
	}
	return false;
}

/* The code below assumes all data is inside range +- this, and that input buffer is single channel */
#define BLENDER_ZMAX 10000.0f

void *NormalizeOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (this->m_cachedInstance == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_imageReader->initializeTileData(rect);
		/* using generic two floats struct to store x: min  y: mult */
		NodeTwoFloats *minmult = new NodeTwoFloats();

		float *buffer = tile->getBuffer();
		int p = tile->getWidth() * tile->getHeight();
		float *bc = buffer;

		float minv = 1.0f + BLENDER_ZMAX;
		float maxv = -1.0f - BLENDER_ZMAX;

		float value;
		while (p--) {
			value = bc[0];
			if ((value > maxv) && (value <= BLENDER_ZMAX)) {
				maxv = value;
			}
			if ((value < minv) && (value >= -BLENDER_ZMAX)) {
				minv = value;
			}
			bc += 4;
		}

		minmult->x = minv;
		/* The rare case of flat buffer  would cause a divide by 0 */
		minmult->y = ((maxv != minv) ? 1.0f / (maxv - minv) : 0.f);

		this->m_cachedInstance = minmult;
	}

	unlockMutex();
	return this->m_cachedInstance;
}

void NormalizeOperation::deinitializeTileData(rcti *rect, void *data)
{
	/* pass */
}
