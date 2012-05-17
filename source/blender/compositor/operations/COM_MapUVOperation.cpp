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

#include "COM_MapUVOperation.h"
#include "BLI_math.h"

MapUVOperation::MapUVOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VECTOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->alpha = 0.f;
	this->setComplex(true);

	this->inputUVProgram = NULL;
	this->inputColorProgram = NULL;
}

void MapUVOperation::initExecution() {
	this->inputColorProgram = this->getInputSocketReader(0);
	this->inputUVProgram = this->getInputSocketReader(1);
}

void MapUVOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputUV[4];
	float uv_a[4], uv_b[4];
	float u,v;

	float dx, dy;
	float uv_l, uv_r;
	float uv_u, uv_d;

	this->inputUVProgram->read(inputUV, x, y, sampler, inputBuffers);
	if (inputUV[2] == 0.f) {
		color[0] = 0.f;
		color[1] = 0.f;
		color[2] = 0.f;
		color[3] = 0.f;
	 return;
	}
	/* adaptive sampling, red (U) channel */
	this->inputUVProgram->read(uv_a, x-1, y, COM_PS_NEAREST, inputBuffers);
	this->inputUVProgram->read(uv_b, x+1, y, COM_PS_NEAREST, inputBuffers);
	uv_l= uv_a[2]!=0.f? fabs(inputUV[0] - uv_a[0]) : 0.f;
	uv_r= uv_b[2]!=0.f? fabs(inputUV[0] - uv_b[0]) : 0.f;

	dx= 0.5f * (uv_l + uv_r);

	/* adaptive sampling, green (V) channel */
	this->inputUVProgram->read(uv_a, x, y-1, COM_PS_NEAREST, inputBuffers);
	this->inputUVProgram->read(uv_b, x, y+1, COM_PS_NEAREST, inputBuffers);
	uv_u= uv_a[2]!=0.f? fabs(inputUV[1] - uv_a[1]) : 0.f;
	uv_d= uv_b[2]!=0.f? fabs(inputUV[1] - uv_b[1]) : 0.f;

	dy= 0.5f * (uv_u + uv_d);

	/* more adaptive sampling, red and green (UV) channels */
	this->inputUVProgram->read(uv_a, x-1, y-1, COM_PS_NEAREST, inputBuffers);
	this->inputUVProgram->read(uv_b, x-1, y+1, COM_PS_NEAREST, inputBuffers);
	uv_l= uv_a[2]!=0.f? fabsf(inputUV[0] - uv_a[0]) : 0.f;
	uv_r= uv_b[2]!=0.f? fabsf(inputUV[0] - uv_b[0]) : 0.f;
	uv_u= uv_a[2]!=0.f? fabsf(inputUV[1] - uv_a[1]) : 0.f;
	uv_d= uv_b[2]!=0.f? fabsf(inputUV[1] - uv_b[1]) : 0.f;

	dx+= 0.25f * (uv_l + uv_r);
	dy+= 0.25f * (uv_u + uv_d);

	this->inputUVProgram->read(uv_a, x+1, y-1, COM_PS_NEAREST, inputBuffers);
	this->inputUVProgram->read(uv_b, x+1, y+1, COM_PS_NEAREST, inputBuffers);
	uv_l= uv_a[2]!=0.f? fabsf(inputUV[0] - uv_a[0]) : 0.f;
	uv_r= uv_b[2]!=0.f? fabsf(inputUV[0] - uv_b[0]) : 0.f;
	uv_u= uv_a[2]!=0.f? fabsf(inputUV[1] - uv_a[1]) : 0.f;
	uv_d= uv_b[2]!=0.f? fabsf(inputUV[1] - uv_b[1]) : 0.f;

	dx+= 0.25f * (uv_l + uv_r);
	dy+= 0.25f * (uv_u + uv_d);

	/* UV to alpha threshold */
	const float threshold = this->alpha * 0.05f;
	float alpha = 1.0f - threshold * (dx + dy);
	if (alpha < 0.f) alpha= 0.f;
	else alpha *= inputUV[2];

	/* should use mipmap */
	dx= min(dx, 0.2f);
	dy= min(dy, 0.2f);


	/* EWA filtering */
	u = inputUV[0] * inputColorProgram->getWidth();
	v = inputUV[1] * inputColorProgram->getHeight();

	this->inputColorProgram->read(color, u, v, dx, dy, inputBuffers);

	/* "premul" */
	if(alpha < 1.0f) {
		color[0]*= alpha;
		color[1]*= alpha;
		color[2]*= alpha;
		color[3]*= alpha;
	}
}

void MapUVOperation::deinitExecution() {
	this->inputUVProgram = NULL;
	this->inputColorProgram = NULL;
}

bool MapUVOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti colorInput;
	rcti uvInput;
	NodeOperation *operation=NULL;

	/* the uv buffer only needs a 3x3 buffer. The image needs whole buffer */

	operation = getInputOperation(0);
	colorInput.xmax = operation->getWidth();
	colorInput.xmin = 0;
	colorInput.ymax = operation->getHeight();
	colorInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
		return true;
	}

	operation = getInputOperation(1);
	uvInput.xmax = input->xmax + 1;
	uvInput.xmin = input->xmin - 1;
	uvInput.ymax = input->ymax + 1;
	uvInput.ymin = input->ymin - 1;
	if (operation->determineDependingAreaOfInterest(&uvInput, readOperation, output)) {
		return true;
	}

	return false;
}

