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

#include "COM_HueSaturationValueCorrectOperation.h"

#include "BLI_math.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

HueSaturationValueCorrectOperation::HueSaturationValueCorrectOperation(): CurveBaseOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->inputProgram = NULL;
}
void HueSaturationValueCorrectOperation::initExecution()
{
	CurveBaseOperation::initExecution();
	this->inputProgram = this->getInputSocketReader(0);
}

void HueSaturationValueCorrectOperation::executePixel(float *output, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float hsv[4], f;

	this->inputProgram->read(hsv, x, y, sampler, inputBuffers);

	/* adjust hue, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 0, hsv[0]);
	hsv[0] += f-0.5f;

	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 1, hsv[0]);
	hsv[1] *= (f * 2.f);

	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 2, hsv[0]);
	hsv[2] *= (f * 2.f);

	hsv[0] = hsv[0] - floor(hsv[0]);  /* mod 1.0 */
	CLAMP(hsv[1], 0.f, 1.f);

	output[0] = hsv[0];
	output[1] = hsv[1];
	output[2] = hsv[2];
	output[3] = hsv[3];
}

void HueSaturationValueCorrectOperation::deinitExecution()
{
	this->inputProgram = NULL;
}
