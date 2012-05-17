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

#include "COM_LensGlowOperation.h"
#include "BLI_math.h"

LensGlowOperation::LensGlowOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputProgram = NULL;
	this->lamp = NULL;
}
void LensGlowOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void LensGlowOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
//	const float emit100 = this->lamp->energy*100;
//	const float emit200 = emit100*2;
//	const float deltaX = 160-x;
//	const float deltaY = 100-y;
//	const float distance = deltaX * deltaX + deltaY*deltaY;

//	float glow = (emit100-(distance))/(emit200);
//	if (glow<0) glow=0;

//	color[0] = glow*lamp->r;
//	color[1] = glow*lamp->g;
//	color[2] = glow*lamp->b;
//	color[3] = 1.0f;
}

void LensGlowOperation::deinitExecution() {
	this->inputProgram = NULL;
}
