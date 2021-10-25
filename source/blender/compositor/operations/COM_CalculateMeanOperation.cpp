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

extern "C" {
#include "IMB_colormanagement.h"
}

CalculateMeanOperation::CalculateMeanOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_imageReader = NULL;
	this->m_iscalculated = false;
	this->m_setting = 1;
	this->setComplex(true);
}
void CalculateMeanOperation::initExecution()
{
	this->m_imageReader = this->getInputSocketReader(0);
	this->m_iscalculated = false;
	NodeOperation::initMutex();
}

void CalculateMeanOperation::executePixel(float output[4],
                                          int /*x*/, int /*y*/,
                                          void * /*data*/)
{
	output[0] = this->m_result;
}

void CalculateMeanOperation::deinitExecution()
{
	this->m_imageReader = NULL;
	NodeOperation::deinitMutex();
}

bool CalculateMeanOperation::determineDependingAreaOfInterest(rcti * /*input*/, ReadBufferOperation *readOperation, rcti *output)
{
	rcti imageInput;
	if (this->m_iscalculated) {
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

void *CalculateMeanOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_iscalculated) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_imageReader->initializeTileData(rect);
		calculateMean(tile);
		this->m_iscalculated = true;
	}
	unlockMutex();
	return NULL;
}

void CalculateMeanOperation::calculateMean(MemoryBuffer *tile)
{
	this->m_result = 0.0f;
	float *buffer = tile->getBuffer();
	int size = tile->getWidth() * tile->getHeight();
	int pixels = 0;
	float sum = 0.0f;
	for (int i = 0, offset = 0; i < size; i++, offset += 4) {
		if (buffer[offset + 3] > 0) {
			pixels++;
	
			switch (this->m_setting) {
				case 1:
				{
					sum += IMB_colormanagement_get_luminance(&buffer[offset]);
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
	this->m_result = sum / pixels;
}
