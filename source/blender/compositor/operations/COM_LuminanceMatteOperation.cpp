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

#include "COM_LuminanceMatteOperation.h"
#include "BLI_math.h"

LuminanceMatteOperation::LuminanceMatteOperation(): NodeOperation() {
	addInputSocket(COM_DT_COLOR);
	addOutputSocket(COM_DT_VALUE);

	inputImageProgram = NULL;
}

void LuminanceMatteOperation::initExecution() {
	this->inputImageProgram = this->getInputSocketReader(0);
}

void LuminanceMatteOperation::deinitExecution() {
	this->inputImageProgram= NULL;
}

void LuminanceMatteOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inColor[4];

	const float high=this->settings->t1;
	const float low=this->settings->t2;

	float alpha;

	this->inputImageProgram->read(inColor, x, y, sampler, inputBuffers);
	
	/* one line thread-friend algorithm:
	outputValue[0] = max(inputValue[3], min(high, max(low, ((inColor[0]-low)/(high-low))))
	*/
		
	/* test range*/
	if(inColor[0] > high) {
		alpha=1.f;
	}
	else if(inColor[0] < low){
		alpha=0.f;
	}
	else {/*blend */
		alpha=(inColor[0]-low)/(high-low);
	}


	/* store matte(alpha) value in [0] to go with
	 * COM_SetAlphaOperation and the Value output
	 */

	/* don't make something that was more transparent less transparent */
	if (alpha<inColor[3]) {
		outputValue[0]=alpha;
	}
	else {
	/* leave now it was before */
		outputValue[0]=inColor[3];
	}
}

