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

#include "COM_GaussianYBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

GaussianYBlurOperation::GaussianYBlurOperation(): BlurBaseOperation()
{
	this->gausstab = NULL;
	this->rad = 0;
}

void *GaussianYBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (!this->sizeavailable) {
		updateGauss(memoryBuffers);
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	return buffer;
}

void GaussianYBlurOperation::initExecution()
{
	if (this->sizeavailable) {
		float rad = size*this->data->sizex;
		if (rad<1)
			rad = 1;

		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);
	}
}

void GaussianYBlurOperation::updateGauss(MemoryBuffer **memoryBuffers)
{
	if (this->gausstab == NULL) {
		updateSize(memoryBuffers);
		float rad = size*this->data->sizey;
		if (rad<1)
			rad = 1;
		
		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);
	}
}

void GaussianYBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float tempColor[4];
	tempColor[0] = 0;
	tempColor[1] = 0;
	tempColor[2] = 0;
	tempColor[3] = 0;
	float overallmultiplyer = 0;
	MemoryBuffer *inputBuffer = (MemoryBuffer*)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	int miny = y - this->rad;
	int maxy = y + this->rad;
	int minx = x;
	int maxx = x;
	miny = max(miny, inputBuffer->getRect()->ymin);
	minx = max(minx, inputBuffer->getRect()->xmin);
	maxy = min(maxy, inputBuffer->getRect()->ymax);
	maxx = min(maxx, inputBuffer->getRect()->xmax);

	int step = getStep();
	int index = 0;
	for (int ny = miny ; ny < maxy ; ny +=step) {
		int bufferindex = ((minx - bufferstartx)*4)+((ny-bufferstarty)*4*bufferwidth);
		float multiplyer = gausstab[index++];
		tempColor[0] += multiplyer * buffer[bufferindex];
		tempColor[1] += multiplyer * buffer[bufferindex+1];
		tempColor[2] += multiplyer * buffer[bufferindex+2];
		tempColor[3] += multiplyer * buffer[bufferindex+3];
		overallmultiplyer += multiplyer;
	}
	float divider = 1.0/overallmultiplyer;
	color[0] = tempColor[0]*divider;
	color[1] = tempColor[1]*divider;
	color[2] = tempColor[2]*divider;
	color[3] = tempColor[3]*divider;
}

void GaussianYBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();
	delete this->gausstab;
	this->gausstab = NULL;
}

bool GaussianYBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti sizeInput;
	sizeInput.xmin = 0;
	sizeInput.ymin = 0;
	sizeInput.xmax = 5;
	sizeInput.ymax = 5;
	
	NodeOperation * operation = this->getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
		return true;
	}
	else {
		if (this->sizeavailable && this->gausstab != NULL) {
			newInput.xmax = input->xmax;
			newInput.xmin = input->xmin;
			newInput.ymax = input->ymax + rad;
			newInput.ymin = input->ymin - rad;
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
