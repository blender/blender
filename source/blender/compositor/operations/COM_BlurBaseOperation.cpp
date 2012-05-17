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

#include "COM_BlurBaseOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

BlurBaseOperation::BlurBaseOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->inputProgram = NULL;
	this->data = NULL;
	this->size = 1.0f;
	this->deleteData = false;
}
void BlurBaseOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	this->inputSize = this->getInputSocketReader(1);
	this->data->image_in_width= this->getWidth();
	this->data->image_in_height= this->getHeight();
	if(this->data->relative) {
		switch (this->data->aspect) {
		case CMP_NODE_BLUR_ASPECT_NONE:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_width);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_height);
			break;
		case CMP_NODE_BLUR_ASPECT_Y:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_width);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_width);
			break;
		case CMP_NODE_BLUR_ASPECT_X:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_height);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_height);
			break;
		}
	}

	QualityStepHelper::initExecution(COM_QH_MULTIPLY);

}

float* BlurBaseOperation::make_gausstab(int rad)
{
	float *gausstab, sum, val;
	int i, n;

	n = 2 * rad + 1;

	gausstab = new float[n];

	sum = 0.0f;
	for (i = -rad; i <= rad; i++) {
		val= RE_filter_value(this->data->filtertype, (float)i/(float)rad);
		sum += val;
		gausstab[i+rad] = val;
	}

	sum= 1.0f/sum;
	for(i=0; i<n; i++)
		gausstab[i]*= sum;

	return gausstab;
}

void BlurBaseOperation::deinitExecution() {
	this->inputProgram = NULL;
	this->inputSize = NULL;
	if (this->deleteData) {
		delete this->data;
	}
	this->data = NULL;
}

void BlurBaseOperation::updateSize(MemoryBuffer **memoryBuffers){
	float result[4];
	this->getInputSocketReader(1)->read(result, 0, 0, COM_PS_NEAREST, memoryBuffers);
	this->size = result[0];
}
