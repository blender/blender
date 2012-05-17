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

#include "COM_GlareThresholdOperation.h"
#include "BLI_math.h"

GlareThresholdOperation::GlareThresholdOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputProgram = NULL;
}
void GlareThresholdOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
}

void GlareThresholdOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	this->inputProgram->read(color, x, y, sampler, inputBuffers);
	if ((0.212671f*color[0] + 0.71516f*color[1] + 0.072169f*color[2]) >= threshold) {
		color[0] -= threshold, color[1] -= threshold, color[2] -= threshold;
		color[0] = MAX2(color[0], 0.f);
		color[1] = MAX2(color[1], 0.f);
		color[2] = MAX2(color[2], 0.f);
	}
	else color[0] = color[1] = color[2] = 0.f;
}

void GlareThresholdOperation::deinitExecution() {
	this->inputProgram = NULL;
}
