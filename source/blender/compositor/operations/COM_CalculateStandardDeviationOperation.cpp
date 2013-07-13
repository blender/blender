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

#include "COM_CalculateStandardDeviationOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"



CalculateStandardDeviationOperation::CalculateStandardDeviationOperation() : CalculateMeanOperation()
{
	/* pass */
}

void CalculateStandardDeviationOperation::executePixel(float output[4], int x, int y, void *data)
{
	output[0] = this->m_standardDeviation;
}

void *CalculateStandardDeviationOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_iscalculated) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_imageReader->initializeTileData(rect);
		CalculateMeanOperation::calculateMean(tile);
		this->m_standardDeviation = 0.0f;
		float *buffer = tile->getBuffer();
		int size = tile->getWidth() * tile->getHeight();
		int pixels = 0;
		float sum = 0.0f;
		float mean = this->m_result;
		for (int i = 0, offset = 0; i < size; i++, offset += 4) {
			if (buffer[offset + 3] > 0) {
				pixels++;
		
				switch (this->m_setting) {
					case 1:  /* rgb combined */
					{
						float value = rgb_to_bw(&buffer[offset]);
						sum += (value - mean) * (value - mean);
						break;
					}
					case 2:  /* red */
					{
						float value = buffer[offset];
						sum += value;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 3:  /* green */
					{
						float value = buffer[offset + 1];
						sum += value;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 4:  /* blue */
					{
						float value = buffer[offset + 2];
						sum += value;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 5:  /* luminance */
					{
						float yuv[3];
						rgb_to_yuv(buffer[offset], buffer[offset + 1], buffer[offset + 2], &yuv[0], &yuv[1], &yuv[2]);
						sum += (yuv[0] - mean) * (yuv[0] - mean);
						break;
					}
				}
			}
		}
		this->m_standardDeviation = sqrt(sum / (float)(pixels - 1));
		this->m_iscalculated = true;
	}
	unlockMutex();
	return NULL;
}
