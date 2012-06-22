/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "COM_CalculateMeanOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"



CalculateMeanOperation::CalculateMeanOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_VALUE);
	this->imageReader = NULL;
	this->iscalculated = false;
	this->setting = 1;
	this->setComplex(true);
}
void CalculateMeanOperation::initExecution()
{
	this->imageReader = this->getInputSocketReader(0);
	this->iscalculated = false;
	NodeOperation::initMutex();
}

void CalculateMeanOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	color[0] = this->result;
}

void CalculateMeanOperation::deinitExecution()
{
	this->imageReader = NULL;
	NodeOperation::deinitMutex();
}

bool CalculateMeanOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti imageInput;
	if (iscalculated) {
		return false;
	}
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

void *CalculateMeanOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	lockMutex();
	if (!this->iscalculated) {
		MemoryBuffer *tile = (MemoryBuffer *)imageReader->initializeTileData(rect, memoryBuffers);
		calculateMean(tile);
		this->iscalculated = true;
	}
	unlockMutex();
	return NULL;
}

void CalculateMeanOperation::calculateMean(MemoryBuffer *tile)
{
	this->result = 0.0f;
	float *buffer = tile->getBuffer();
	int size = tile->getWidth() * tile->getHeight();
	int pixels = 0;
	float sum = 0.0f;
	for (int i = 0, offset = 0; i < size; i++, offset += 4) {
		if (buffer[offset + 3] > 0) {
			pixels++;
	
			switch (this->setting)
			{
				case 1:
				{
					sum += rgb_to_bw(&buffer[offset]);
					break;
				}
				case 2:
				{
					sum += buffer[offset];
					break;
				}
				case 3:
				{
					sum += buffer[offset + 1];
					break;
				}
				case 4:
				{
					sum += buffer[offset + 2];
					break;
				}
				case 5:
				{
					float yuv[3];
					rgb_to_yuv(buffer[offset], buffer[offset + 1], buffer[offset + 2], &yuv[0], &yuv[1], &yuv[2]);
					sum += yuv[0];
					break;
				}
			}
		}
	}
	this->result = sum / pixels;
}
