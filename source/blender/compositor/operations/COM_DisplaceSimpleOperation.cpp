/*
 * Copyright 2012, Blender Foundation.
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

#include "COM_DisplaceSimpleOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

DisplaceSimpleOperation::DisplaceSimpleOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VECTOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);

	this->m_inputColorProgram = NULL;
	this->m_inputVectorProgram = NULL;
	this->m_inputScaleXProgram = NULL;
	this->m_inputScaleYProgram = NULL;
}

void DisplaceSimpleOperation::initExecution()
{
	this->m_inputColorProgram = this->getInputSocketReader(0);
	this->m_inputVectorProgram = this->getInputSocketReader(1);
	this->m_inputScaleXProgram = this->getInputSocketReader(2);
	this->m_inputScaleYProgram = this->getInputSocketReader(3);

	this->m_width_x4 = this->getWidth() * 4;
	this->m_height_x4 = this->getHeight() * 4;
}


/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
#define DISPLACE_EPSILON    0.01f

void DisplaceSimpleOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	float inVector[4];
	float inScale[4];

	float p_dx, p_dy;   /* main displacement in pixel space */
	float u, v;

	this->m_inputScaleXProgram->read(inScale, x, y, sampler);
	float xs = inScale[0];
	this->m_inputScaleYProgram->read(inScale, x, y, sampler);
	float ys = inScale[0];

	/* clamp x and y displacement to triple image resolution - 
	 * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
	CLAMP(xs, -this->m_width_x4, this->m_width_x4);
	CLAMP(ys, -this->m_height_x4, this->m_height_x4);

	this->m_inputVectorProgram->read(inVector, x, y, sampler);
	p_dx = inVector[0] * xs;
	p_dy = inVector[1] * ys;

	/* displaced pixel in uv coords, for image sampling */
	/* clamp nodes to avoid glitches */
	u = x - p_dx + 0.5f;
	v = y - p_dy + 0.5f;
	CLAMP(u, 0.f, this->getWidth() - 1.f);
	CLAMP(v, 0.f, this->getHeight() - 1.f);

	this->m_inputColorProgram->read(color, u, v, sampler);
}

void DisplaceSimpleOperation::deinitExecution()
{
	this->m_inputColorProgram = NULL;
	this->m_inputVectorProgram = NULL;
	this->m_inputScaleXProgram = NULL;
	this->m_inputScaleYProgram = NULL;
}

bool DisplaceSimpleOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti colorInput;
	NodeOperation *operation = NULL;

	/* the vector buffer only needs a 2x2 buffer. The image needs whole buffer */
	/* image */
	operation = getInputOperation(0);
	colorInput.xmax = operation->getWidth();
	colorInput.xmin = 0;
	colorInput.ymax = operation->getHeight();
	colorInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
		return true;
	}

	/* vector */
	if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
		return true;
	}

	/* scale x */
	operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}

	/* scale y */
	operation = getInputOperation(3);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}

	return false;
}

