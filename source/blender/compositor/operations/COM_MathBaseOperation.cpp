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

#include "COM_MathBaseOperation.h"
extern "C" {
#include "BLI_math.h"
}

MathBaseOperation::MathBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_inputValue1Operation = NULL;
	this->m_inputValue2Operation = NULL;
	this->m_useClamp = false;
}

void MathBaseOperation::initExecution()
{
	this->m_inputValue1Operation = this->getInputSocketReader(0);
	this->m_inputValue2Operation = this->getInputSocketReader(1);
}


void MathBaseOperation::deinitExecution()
{
	this->m_inputValue1Operation = NULL;
	this->m_inputValue2Operation = NULL;
}

void MathBaseOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	InputSocket *socket;
	unsigned int tempPreferredResolution[2] = {0, 0};
	unsigned int tempResolution[2];

	socket = this->getInputSocket(0);
	socket->determineResolution(tempResolution, tempPreferredResolution);
	if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
		this->setResolutionInputSocketIndex(0);
	}
	else {
		this->setResolutionInputSocketIndex(1);
	}
	NodeOperation::determineResolution(resolution, preferredResolution);
}

void MathBaseOperation::clampIfNeeded(float *color)
{
	if (this->m_useClamp) {
		CLAMP(color[0], 0.0f, 1.0f);
	}
}

void MathAddOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = inputValue1[0] + inputValue2[0];

	clampIfNeeded(output);
}

void MathSubtractOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = inputValue1[0] - inputValue2[0];

	clampIfNeeded(output);
}

void MathMultiplyOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = inputValue1[0] * inputValue2[0];

	clampIfNeeded(output);
}

void MathDivideOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue2[0] == 0) /* We don't want to divide by zero. */
		output[0] = 0.0;
	else
		output[0] = inputValue1[0] / inputValue2[0];

	clampIfNeeded(output);
}

void MathSineOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = sin(inputValue1[0]);

	clampIfNeeded(output);
}

void MathCosineOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = cos(inputValue1[0]);

	clampIfNeeded(output);
}

void MathTangentOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = tan(inputValue1[0]);

	clampIfNeeded(output);
}

void MathArcSineOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue1[0] <= 1 && inputValue1[0] >= -1)
		output[0] = asin(inputValue1[0]);
	else
		output[0] = 0.0;

	clampIfNeeded(output);
}

void MathArcCosineOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue1[0] <= 1 && inputValue1[0] >= -1)
		output[0] = acos(inputValue1[0]);
	else
		output[0] = 0.0;

	clampIfNeeded(output);
}

void MathArcTangentOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = atan(inputValue1[0]);

	clampIfNeeded(output);
}

void MathPowerOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue1[0] >= 0) {
		output[0] = pow(inputValue1[0], inputValue2[0]);
	}
	else {
		float y_mod_1 = fmod(inputValue2[0], 1);
		/* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
		if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
			output[0] = pow(inputValue1[0], floorf(inputValue2[0] + 0.5f));
		}
		else {
			output[0] = 0.0;
		}
	}

	clampIfNeeded(output);
}

void MathLogarithmOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue1[0] > 0  && inputValue2[0] > 0)
		output[0] = log(inputValue1[0]) / log(inputValue2[0]);
	else
		output[0] = 0.0;

	clampIfNeeded(output);
}

void MathMinimumOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = min(inputValue1[0], inputValue2[0]);

	clampIfNeeded(output);
}

void MathMaximumOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = max(inputValue1[0], inputValue2[0]);

	clampIfNeeded(output);
}

void MathRoundOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = round(inputValue1[0]);

	clampIfNeeded(output);
}

void MathLessThanOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = inputValue1[0] < inputValue2[0] ? 1.0f : 0.0f;

	clampIfNeeded(output);
}

void MathGreaterThanOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	output[0] = inputValue1[0] > inputValue2[0] ? 1.0f : 0.0f;

	clampIfNeeded(output);
}

void MathModuloOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->readSampled(inputValue1, x, y, sampler);
	this->m_inputValue2Operation->readSampled(inputValue2, x, y, sampler);
	
	if (inputValue2[0] == 0)
		output[0] = 0.0;
	else
		output[0] = fmod(inputValue1[0], inputValue2[0]);

	clampIfNeeded(output);
}

