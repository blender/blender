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

void MathBaseOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	InputSocket *socket;
	unsigned int tempPreferredResolution[] = {0, 0};
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

void MathAddOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = inputValue1[0] + inputValue2[0];

	clampIfNeeded(outputValue);
}

void MathSubtractOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = inputValue1[0] - inputValue2[0];

	clampIfNeeded(outputValue);
}

void MathMultiplyOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = inputValue1[0] * inputValue2[0];

	clampIfNeeded(outputValue);
}

void MathDivideOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	if (inputValue2[0] == 0) /* We don't want to divide by zero. */
		outputValue[0] = 0.0;
	else
		outputValue[0] = inputValue1[0] / inputValue2[0];

	clampIfNeeded(outputValue);
}

void MathSineOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = sin(inputValue1[0]);

	clampIfNeeded(outputValue);
}

void MathCosineOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = cos(inputValue1[0]);

	clampIfNeeded(outputValue);
}

void MathTangentOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = tan(inputValue1[0]);

	clampIfNeeded(outputValue);
}

void MathArcSineOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	if (inputValue1[0] <= 1 && inputValue1[0] >= -1)
		outputValue[0] = asin(inputValue1[0]);
	else
		outputValue[0] = 0.0;

	clampIfNeeded(outputValue);
}

void MathArcCosineOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	if (inputValue1[0] <= 1 && inputValue1[0] >= -1)
		outputValue[0] = acos(inputValue1[0]);
	else
		outputValue[0] = 0.0;

	clampIfNeeded(outputValue);
}

void MathArcTangentOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = atan(inputValue1[0]);

	clampIfNeeded(outputValue);
}

void MathPowerOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	if (inputValue1[0] >= 0) {
		outputValue[0] = pow(inputValue1[0], inputValue2[0]);
	}
	else {
		float y_mod_1 = fmod(inputValue2[0], 1);
		/* if input value is not nearly an integer, fall back to zero, nicer than straight rounding */
		if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
			outputValue[0] = pow(inputValue1[0], floorf(inputValue2[0] + 0.5f));
		}
		else {
			outputValue[0] = 0.0;
		}
	}

	clampIfNeeded(outputValue);
}

void MathLogarithmOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	if (inputValue1[0] > 0  && inputValue2[0] > 0)
		outputValue[0] = log(inputValue1[0]) / log(inputValue2[0]);
	else
		outputValue[0] = 0.0;

	clampIfNeeded(outputValue);
}

void MathMinimumOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = min(inputValue1[0], inputValue2[0]);

	clampIfNeeded(outputValue);
}

void MathMaximumOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = max(inputValue1[0], inputValue2[0]);

	clampIfNeeded(outputValue);
}

void MathRoundOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = round(inputValue1[0]);

	clampIfNeeded(outputValue);
}

void MathLessThanOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = inputValue1[0] < inputValue2[0] ? 1.0f : 0.0f;

	clampIfNeeded(outputValue);
}

void MathGreaterThanOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	float inputValue1[4];
	float inputValue2[4];
	
	this->m_inputValue1Operation->read(&inputValue1[0], x, y, sampler);
	this->m_inputValue2Operation->read(&inputValue2[0], x, y, sampler);
	
	outputValue[0] = inputValue1[0] > inputValue2[0] ? 1.0f : 0.0f;

	clampIfNeeded(outputValue);
}


