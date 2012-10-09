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

#include "MEM_guardedalloc.h"

ConvolutionFilterOperation::ConvolutionFilterOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputOperation = NULL;
	this->m_filter = NULL;
	this->setComplex(true);
}
void ConvolutionFilterOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	this->m_inputValueOperation = this->getInputSocketReader(1);
}

void ConvolutionFilterOperation::set3x3Filter(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9)
{
	this->m_filter = (float *)MEM_mallocN(sizeof(float) * 9, __func__);
	this->m_filter[0] = f1;
	this->m_filter[1] = f2;
	this->m_filter[2] = f3;
	this->m_filter[3] = f4;
	this->m_filter[4] = f5;
	this->m_filter[5] = f6;
	this->m_filter[6] = f7;
	this->m_filter[7] = f8;
	this->m_filter[8] = f9;
	this->m_filterHeight = 3;
	this->m_filterWidth = 3;
}

void ConvolutionFilterOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
	this->m_inputValueOperation = NULL;
	if (this->m_filter) {
		MEM_freeN(this->m_filter);
		this->m_filter = NULL;
	}
}


void ConvolutionFilterOperation::executePixel(float output[4], int x, int y, void *data)
{
	float in1[4];
	float in2[4];
	int x1 = x - 1;
	int x2 = x;
	int x3 = x + 1;
	int y1 = y - 1;
	int y2 = y;
	int y3 = y + 1;
	CLAMP(x1, 0, getWidth() - 1);
	CLAMP(x2, 0, getWidth() - 1);
	CLAMP(x3, 0, getWidth() - 1);
	CLAMP(y1, 0, getHeight() - 1);
	CLAMP(y2, 0, getHeight() - 1);
	CLAMP(y3, 0, getHeight() - 1);
	float value[4];
	this->m_inputValueOperation->read(value, x2, y2, NULL);
	const float mval = 1.0f - value[0];

	zero_v4(output);
	this->m_inputOperation->read(in1, x1, y1, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[0]);
	this->m_inputOperation->read(in1, x2, y1, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[1]);
	this->m_inputOperation->read(in1, x3, y1, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[2]);
	this->m_inputOperation->read(in1, x1, y2, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[3]);
	this->m_inputOperation->read(in2, x2, y2, NULL);
	madd_v4_v4fl(output, in2, this->m_filter[4]);
	this->m_inputOperation->read(in1, x3, y2, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[5]);
	this->m_inputOperation->read(in1, x1, y3, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[6]);
	this->m_inputOperation->read(in1, x2, y3, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[7]);
	this->m_inputOperation->read(in1, x3, y3, NULL);
	madd_v4_v4fl(output, in1, this->m_filter[8]);
	
	output[0] = output[0] * value[0] + in2[0] * mval;
	output[1] = output[1] * value[0] + in2[1] * mval;
	output[2] = output[2] * value[0] + in2[2] * mval;
	output[3] = output[3] * value[0] + in2[3] * mval;
}

bool ConvolutionFilterOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	int addx = (this->m_filterWidth - 1) / 2 + 1;
	int addy = (this->m_filterHeight - 1) / 2 + 1;
	newInput.xmax = input->xmax + addx;
	newInput.xmin = input->xmin - addx;
	newInput.ymax = input->ymax + addy;
	newInput.ymin = input->ymin - addy;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
