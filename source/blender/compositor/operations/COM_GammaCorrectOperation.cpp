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

#include "COM_GammaCorrectOperation.h"
#include "BLI_math.h"

GammaCorrectOperation::GammaCorrectOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputProgram = NULL;
}
void GammaCorrectOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
}

void GammaCorrectOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor[4];
	this->m_inputProgram->readSampled(inputColor, x, y, sampler);
	if (inputColor[3] > 0.0f) {
		inputColor[0] /= inputColor[3];
		inputColor[1] /= inputColor[3];
		inputColor[2] /= inputColor[3];
	}

	/* check for negative to avoid nan's */
	output[0] = inputColor[0] > 0.0f ? inputColor[0] * inputColor[0] : 0.0f;
	output[1] = inputColor[1] > 0.0f ? inputColor[1] * inputColor[1] : 0.0f;
	output[2] = inputColor[2] > 0.0f ? inputColor[2] * inputColor[2] : 0.0f;
	output[3] = inputColor[3];

	if (inputColor[3] > 0.0f) {
		output[0] *= inputColor[3];
		output[1] *= inputColor[3];
		output[2] *= inputColor[3];
	}
}

void GammaCorrectOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
}

GammaUncorrectOperation::GammaUncorrectOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputProgram = NULL;
}
void GammaUncorrectOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
}

void GammaUncorrectOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor[4];
	this->m_inputProgram->readSampled(inputColor, x, y, sampler);

	if (inputColor[3] > 0.0f) {
		inputColor[0] /= inputColor[3];
		inputColor[1] /= inputColor[3];
		inputColor[2] /= inputColor[3];
	}

	output[0] = inputColor[0] > 0.0f ? sqrtf(inputColor[0]) : 0.0f;
	output[1] = inputColor[1] > 0.0f ? sqrtf(inputColor[1]) : 0.0f;
	output[2] = inputColor[2] > 0.0f ? sqrtf(inputColor[2]) : 0.0f;
	output[3] = inputColor[3];

	if (inputColor[3] > 0.0f) {
		output[0] *= inputColor[3];
		output[1] *= inputColor[3];
		output[2] *= inputColor[3];
	}
}

void GammaUncorrectOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
}
