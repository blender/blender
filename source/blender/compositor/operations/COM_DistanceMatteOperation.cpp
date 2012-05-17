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
 *		Dalai Felinto
 */

#include "COM_DistanceMatteOperation.h"
#include "BLI_math.h"

DistanceMatteOperation::DistanceMatteOperation(): NodeOperation()
{
	addInputSocket(COM_DT_COLOR);
	addInputSocket(COM_DT_COLOR);
	addOutputSocket(COM_DT_VALUE);

	inputImageProgram = NULL;
	inputKeyProgram = NULL;
}

void DistanceMatteOperation::initExecution()
{
	this->inputImageProgram = this->getInputSocketReader(0);
	this->inputKeyProgram = this->getInputSocketReader(1);
}

void DistanceMatteOperation::deinitExecution()
{
	this->inputImageProgram = NULL;
	this->inputKeyProgram = NULL;
}

void DistanceMatteOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inKey[4];
	float inImage[4];

	const float tolerence=this->settings->t1;
	const float falloff=this->settings->t2;

	float distance;
	float alpha;

	this->inputKeyProgram->read(inKey, x, y, sampler, inputBuffers);
	this->inputImageProgram->read(inImage, x, y, sampler, inputBuffers);
	
	distance = sqrt(pow((inKey[0]-inImage[0]),2)+
		pow((inKey[1]-inImage[1]),2)+
		pow((inKey[2]-inImage[2]),2));

	/* store matte(alpha) value in [0] to go with
	 * COM_SetAlphaOperation and the Value output
	 */
 
	/*make 100% transparent */
	if (distance < tolerence) {
		outputValue[0]=0.f;
	}
	/*in the falloff region, make partially transparent */
	else if (distance < falloff+tolerence) {
		distance=distance-tolerence;
		alpha=distance/falloff;
		/*only change if more transparent than before */
		if (alpha < inImage[3]) {
			outputValue[0]=alpha;
		}
		else { /* leave as before */
			outputValue[0]=inImage[3];
		}
	}
	else {
	/* leave as before */
		outputValue[0]=inImage[3];
	}
}

