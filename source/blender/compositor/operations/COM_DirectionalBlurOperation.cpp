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

#include "COM_DirectionalBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

DirectionalBlurOperation::DirectionalBlurOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->inputProgram = NULL;
}

void DirectionalBlurOperation::initExecution() {
	this->inputProgram = getInputSocketReader(0);
	QualityStepHelper::initExecution(COM_QH_INCREASE);
	const float angle = this->data->angle;
	const float zoom = this->data->zoom;
	const float spin = this->data->spin;
	const float iterations = this->data->iter;
	const float distance = this->data->distance;
	const float center_x = this->data->center_x;
	const float center_y = this->data->center_y;
	const float width = getWidth();
	const float height = getHeight();

	const float a= angle;
	const float itsc= 1.f / pow(2.f, (float)iterations);
	float D;

	D= distance * sqrtf(width*width + height*height);
	center_x_pix= center_x * width;
	center_y_pix= center_y * height;

	tx=  itsc * D * cos(a);
	ty= -itsc * D * sin(a);
	sc=  itsc * zoom;
	rot= itsc * spin;

}

void DirectionalBlurOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	const int iterations = pow(2.f, this->data->iter);
	float col[4]= {0,0,0,0};
	float col2[4]= {0,0,0,0};
	this->inputProgram->read(col2, x, y, COM_PS_NEAREST, inputBuffers);
	float ltx = tx;
	float lty = ty;
	float lsc = sc;
	float lrot = rot;
	/* blur the image */
	for(int i= 0; i < iterations; ++i) {
		const float cs= cos(lrot), ss= sin(lrot);
		const float isc= 1.f / (1.f + lsc);

		const float v= isc * (y - center_y_pix) + lty;
		const float u= isc * (x - center_x_pix) + ltx;

		this->inputProgram->read(col, cs * u + ss * v + center_x_pix, cs * v - ss * u + center_y_pix, COM_PS_NEAREST, inputBuffers);

		col2[0] += col[0];
		col2[1] += col[1];
		col2[2] += col[2];
		col2[3] += col[3];

		/* double transformations */
		ltx += tx;
		lty += ty;
		lrot += rot;
		lsc += sc;
	}
	color[0] = col2[0]/iterations;
	color[1] = col2[1]/iterations;
	color[2] = col2[2]/iterations;
	color[3] = col2[3]/iterations;
}

void DirectionalBlurOperation::deinitExecution() {
	this->inputProgram = NULL;
}

bool DirectionalBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;

	newInput.xmax = this->getWidth();
	newInput.xmin = 0;
	newInput.ymax = this->getHeight();
	newInput.ymin = 0;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
