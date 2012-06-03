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

#include "COM_ConvolutionFilterOperation.h"

#include "BLI_utildefines.h"

ConvolutionFilterOperation::ConvolutionFilterOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
	this->filter = NULL;
	this->setComplex(true);
}
void ConvolutionFilterOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
	this->inputValueOperation = this->getInputSocketReader(1);
}

void ConvolutionFilterOperation::set3x3Filter(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9)
{
	this->filter = new float[9];
	this->filter[0] = f1;
	this->filter[1] = f2;
	this->filter[2] = f3;
	this->filter[3] = f4;
	this->filter[4] = f5;
	this->filter[5] = f6;
	this->filter[6] = f7;
	this->filter[7] = f8;
	this->filter[8] = f9;
	this->filterHeight = 3;
	this->filterWidth = 3;
}

void ConvolutionFilterOperation::deinitExecution()
{
	this->inputOperation = NULL;
	this->inputValueOperation = NULL;
	if (this->filter) {
		delete this->filter;
		this->filter = NULL;
	}
}


void ConvolutionFilterOperation::executePixel(float *color,int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	color[0] = 0.0;
	color[1] = 0.0;
	color[2] = 0.0;
	color[3] = 0.0;
	float in1[4];
	float in2[4];
	int x1 = x - 1;
	int x2 = x;
	int x3 = x + 1;
	int y1 = y - 1;
	int y2 = y;
	int y3 = y + 1;
	CLAMP(x1, 0, getWidth()-1);
	CLAMP(x2, 0, getWidth()-1);
	CLAMP(x3, 0, getWidth()-1);
	CLAMP(y1, 0, getHeight()-1);
	CLAMP(y2, 0, getHeight()-1);
	CLAMP(y3, 0, getHeight()-1);
	float value[4];
	this->inputValueOperation->read(value, x2, y2, inputBuffers, NULL);
	float mval = 1.0f - value[0];
	this->inputOperation->read(in1, x1, y1, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[0];
	color[1] += in1[1] * this->filter[0];
	color[2] += in1[2] * this->filter[0];
	color[3] += in1[3] * this->filter[0];
	this->inputOperation->read(in1, x2, y1, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[1];
	color[1] += in1[1] * this->filter[1];
	color[2] += in1[2] * this->filter[1];
	color[3] += in1[3] * this->filter[1];
	this->inputOperation->read(in1, x3, y1, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[2];
	color[1] += in1[1] * this->filter[2];
	color[2] += in1[2] * this->filter[2];
	color[3] += in1[3] * this->filter[2];
	this->inputOperation->read(in1, x1, y2, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[3];
	color[1] += in1[1] * this->filter[3];
	color[2] += in1[2] * this->filter[3];
	color[3] += in1[3] * this->filter[3];
	this->inputOperation->read(in2, x2, y2, inputBuffers, NULL);
	color[0] += in2[0] * this->filter[4];
	color[1] += in2[1] * this->filter[4];
	color[2] += in2[2] * this->filter[4];
	color[3] += in2[3] * this->filter[4];
	this->inputOperation->read(in1, x3, y2, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[5];
	color[1] += in1[1] * this->filter[5];
	color[2] += in1[2] * this->filter[5];
	color[3] += in1[3] * this->filter[5];
	this->inputOperation->read(in1, x1, y3, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[6];
	color[1] += in1[1] * this->filter[6];
	color[2] += in1[2] * this->filter[6];
	color[3] += in1[3] * this->filter[6];
	this->inputOperation->read(in1, x2, y3, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[7];
	color[1] += in1[1] * this->filter[7];
	color[2] += in1[2] * this->filter[7];
	color[3] += in1[3] * this->filter[7];
	this->inputOperation->read(in1, x3, y3, inputBuffers, NULL);
	color[0] += in1[0] * this->filter[8];
	color[1] += in1[1] * this->filter[8];
	color[2] += in1[2] * this->filter[8];
	color[3] += in1[3] * this->filter[8];
	
	color[0] = color[0]*value[0] + in2[0] * mval;
	color[1] = color[1]*value[0] + in2[1] * mval;
	color[2] = color[2]*value[0] + in2[2] * mval;
	color[3] = color[3]*value[0] + in2[3] * mval;
}

bool ConvolutionFilterOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	int addx = (this->filterWidth-1)/2+1;
	int addy = (this->filterHeight-1)/2+1;
	newInput.xmax = input->xmax + addx;
	newInput.xmin = input->xmin - addx;
	newInput.ymax = input->ymax + addy;
	newInput.ymin = input->ymin - addy;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
