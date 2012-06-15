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

#include "COM_GaussianXBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

GaussianXBlurOperation::GaussianXBlurOperation() : BlurBaseOperation()
{
	this->gausstab = NULL;
	this->rad = 0;

}

void *GaussianXBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (!this->sizeavailable) {
		updateGauss(memoryBuffers);
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void GaussianXBlurOperation::initExecution()
{
	BlurBaseOperation::initExecution();

	if (this->sizeavailable) {
		float rad = size * this->data->sizex;
		if (rad < 1)
			rad = 1;

		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);
	}
}

void GaussianXBlurOperation::updateGauss(MemoryBuffer **memoryBuffers)
{
	if (this->gausstab == NULL) {
		updateSize(memoryBuffers);
		float rad = size * this->data->sizex;
		if (rad < 1)
			rad = 1;

		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);	
	}	
}

void GaussianXBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float tempColor[4];
	tempColor[0] = 0;
	tempColor[1] = 0;
	tempColor[2] = 0;
	tempColor[3] = 0;
	float overallmultiplyer = 0.0f;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	int miny = y;
	int maxy = y;
	int minx = x - this->rad;
	int maxx = x + this->rad;
	miny = max(miny, inputBuffer->getRect()->ymin);
	minx = max(minx, inputBuffer->getRect()->xmin);
	maxy = min(maxy, inputBuffer->getRect()->ymax);
	maxx = min(maxx, inputBuffer->getRect()->xmax);

	int index;
	int step = getStep();
	int offsetadd = getOffsetAdd();
	int bufferindex = ((minx - bufferstartx) * 4) + ((miny - bufferstarty) * 4 * bufferwidth);
	for (int nx = minx; nx < maxx; nx += step) {
		index = (nx - x) + this->rad;
		const float multiplyer = gausstab[index];
		madd_v4_v4fl(tempColor, &buffer[bufferindex], multiplyer);
		overallmultiplyer += multiplyer;
		bufferindex += offsetadd;
	}
	mul_v4_v4fl(color, tempColor, 1.0f / overallmultiplyer);
}

void GaussianXBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();
	delete this->gausstab;
	this->gausstab = NULL;
}

bool GaussianXBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti sizeInput;
	sizeInput.xmin = 0;
	sizeInput.ymin = 0;
	sizeInput.xmax = 5;
	sizeInput.ymax = 5;
	
	NodeOperation *operation = this->getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
		return true;
	}
	else {
		if (this->sizeavailable && this->gausstab != NULL) {
			newInput.xmax = input->xmax + rad;
			newInput.xmin = input->xmin - rad;
			newInput.ymax = input->ymax;
			newInput.ymin = input->ymin;
		}
		else {
			newInput.xmax = this->getWidth();
			newInput.xmin = 0;
			newInput.ymax = this->getHeight();
			newInput.ymin = 0;
		}
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}
