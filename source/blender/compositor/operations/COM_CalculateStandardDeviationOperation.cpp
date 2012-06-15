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

void CalculateStandardDeviationOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	color[0] = this->standardDeviation;
}

void *CalculateStandardDeviationOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	lockMutex();
	if (!this->iscalculated) {
		MemoryBuffer *tile = (MemoryBuffer *)imageReader->initializeTileData(rect, memoryBuffers);
		CalculateMeanOperation::calculateMean(tile);
		this->standardDeviation = 0.0f;
		float *buffer = tile->getBuffer();
		int size = tile->getWidth() * tile->getHeight();
		int pixels = 0;
		float sum;
		float mean = this->result;
		for (int i = 0, offset = 0; i < size; i++, offset += 4) {
			if (buffer[offset + 3] > 0) {
				pixels++;
		
				switch (this->setting)
				{
					case 1:
					{
						float value = buffer[offset] * 0.35f + buffer[offset + 1] * 0.45f + buffer[offset + 2] * 0.2f;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 2:
					{
						float value = buffer[offset];
						sum += value;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 3:
					{
						float value = buffer[offset + 1];
						sum += value;
						sum += (value - mean) * (value - mean);
						break;
					}
					case 4:
					{
						float value = buffer[offset + 2];
						sum += value;
						sum += (value - mean) * (value - mean);
					}
					case 5:
					{
						float yuv[3];
						rgb_to_yuv(buffer[offset], buffer[offset + 1], buffer[offset + 2], &yuv[0], &yuv[1], &yuv[2]);
						sum += (yuv[0] - mean) * (yuv[0] - mean);
						break;
					}
				}
			}
		}
		this->standardDeviation = sqrt(sum / (float)(pixels - 1));
		this->iscalculated = true;
	}
	unlockMutex();
	return NULL;
}
