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

#include "COM_VectorCurveOperation.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

VectorCurveOperation::VectorCurveOperation(): CurveBaseOperation() {
	this->addInputSocket(COM_DT_VECTOR);
	this->addOutputSocket(COM_DT_VECTOR);

	this->inputProgram = NULL;
}
void VectorCurveOperation::initExecution() {
	CurveBaseOperation::initExecution();
	this->inputProgram = this->getInputSocketReader(0);
}

void VectorCurveOperation::executePixel(float* output, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float input[4];


	this->inputProgram->read(input, x, y, sampler, inputBuffers);

	curvemapping_evaluate_premulRGBF(this->curveMapping, output, input);
	output[3]= input[3];
}

void VectorCurveOperation::deinitExecution() {
	this->inputProgram = NULL;
}
