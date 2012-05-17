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

#include "COM_ColorCurveOperation.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

ColorCurveOperation::ColorCurveOperation(): CurveBaseOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->inputFacProgram = NULL;
	this->inputImageProgram = NULL;
	this->inputBlackProgram = NULL;
	this->inputWhiteProgram = NULL;

	this->setResolutionInputSocketIndex(1);
}
void ColorCurveOperation::initExecution() {
	CurveBaseOperation::initExecution();
	this->inputFacProgram = this->getInputSocketReader(0);
	this->inputImageProgram = this->getInputSocketReader(1);
	this->inputBlackProgram = this->getInputSocketReader(2);
	this->inputWhiteProgram = this->getInputSocketReader(3);

	curvemapping_premultiply(this->curveMapping, 0);

}

void ColorCurveOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float black[4];
	float white[4];
	float fac[4];
	float image[4];

	this->inputBlackProgram->read(black, x, y, sampler, inputBuffers);
	this->inputWhiteProgram->read(white, x, y, sampler, inputBuffers);

	curvemapping_set_black_white(this->curveMapping, black, white);

	this->inputFacProgram->read(fac, x, y, sampler, inputBuffers);
	this->inputImageProgram->read(image, x, y, sampler, inputBuffers);

	if(fac[0]>=1.0)
		curvemapping_evaluate_premulRGBF(this->curveMapping, color, image);
	else if(*fac<=0.0) {
		color[0]= image[0];
		color[1]= image[1];
		color[2]= image[2];
	}
	else {
		float col[4], mfac= 1.0f-*fac;
		curvemapping_evaluate_premulRGBF(this->curveMapping, col, image);
		color[0]= mfac*image[0] + *fac*col[0];
		color[1]= mfac*image[1] + *fac*col[1];
		color[2]= mfac*image[2] + *fac*col[2];
	}
	color[3]= image[3];
}

void ColorCurveOperation::deinitExecution() {
	this->inputFacProgram = NULL;
	this->inputImageProgram = NULL;
	this->inputBlackProgram = NULL;
	this->inputWhiteProgram = NULL;
	curvemapping_premultiply(this->curveMapping, 1);
}
