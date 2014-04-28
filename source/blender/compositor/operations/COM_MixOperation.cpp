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

#include "COM_MixOperation.h"

extern "C" {
#  include "BLI_math.h"
}

/* ******** Mix Base Operation ******** */

MixBaseOperation::MixBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputValueOperation = NULL;
	this->m_inputColor1Operation = NULL;
	this->m_inputColor2Operation = NULL;
	this->setUseValueAlphaMultiply(false);
	this->setUseClamp(false);
}

void MixBaseOperation::initExecution()
{
	this->m_inputValueOperation = this->getInputSocketReader(0);
	this->m_inputColor1Operation = this->getInputSocketReader(1);
	this->m_inputColor2Operation = this->getInputSocketReader(2);
}

void MixBaseOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	
	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	
	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	output[0] = valuem * (inputColor1[0]) + value * (inputColor2[0]);
	output[1] = valuem * (inputColor1[1]) + value * (inputColor2[1]);
	output[2] = valuem * (inputColor1[2]) + value * (inputColor2[2]);
	output[3] = inputColor1[3];
}

void MixBaseOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	NodeOperationInput *socket;
	unsigned int tempPreferredResolution[2] = {0, 0};
	unsigned int tempResolution[2];

	socket = this->getInputSocket(1);
	socket->determineResolution(tempResolution, tempPreferredResolution);
	if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
		this->setResolutionInputSocketIndex(1);
	}
	else {
		socket = this->getInputSocket(2);
		socket->determineResolution(tempResolution, tempPreferredResolution);
		if ((tempResolution[0] != 0) && (tempResolution[1] != 0)) {
			this->setResolutionInputSocketIndex(2);
		}
		else {
			this->setResolutionInputSocketIndex(0);
		}
	}
	NodeOperation::determineResolution(resolution, preferredResolution);
}

void MixBaseOperation::deinitExecution()
{
	this->m_inputValueOperation = NULL;
	this->m_inputColor1Operation = NULL;
	this->m_inputColor2Operation = NULL;
}

/* ******** Mix Add Operation ******** */

MixAddOperation::MixAddOperation() : MixBaseOperation()
{
	/* pass */
}

void MixAddOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	output[0] = inputColor1[0] + value * inputColor2[0];
	output[1] = inputColor1[1] + value * inputColor2[1];
	output[2] = inputColor1[2] + value * inputColor2[2];
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Blend Operation ******** */

MixBlendOperation::MixBlendOperation() : MixBaseOperation()
{
	/* pass */
}

void MixBlendOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float value;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	value = inputValue[0];
	
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	output[0] = valuem * (inputColor1[0]) + value * (inputColor2[0]);
	output[1] = valuem * (inputColor1[1]) + value * (inputColor2[1]);
	output[2] = valuem * (inputColor1[2]) + value * (inputColor2[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Burn Operation ******** */

MixBurnOperation::MixBurnOperation() : MixBaseOperation()
{
	/* pass */
}

void MixBurnOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float tmp;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	tmp = valuem + value * inputColor2[0];
	if (tmp <= 0.0f)
		output[0] = 0.0f;
	else {
		tmp = 1.0f - (1.0f - inputColor1[0]) / tmp;
		if (tmp < 0.0f)
			output[0] = 0.0f;
		else if (tmp > 1.0f)
			output[0] = 1.0f;
		else
			output[0] = tmp;
	}

	tmp = valuem + value * inputColor2[1];
	if (tmp <= 0.0f)
		output[1] = 0.0f;
	else {
		tmp = 1.0f - (1.0f - inputColor1[1]) / tmp;
		if (tmp < 0.0f)
			output[1] = 0.0f;
		else if (tmp > 1.0f)
			output[1] = 1.0f;
		else
			output[1] = tmp;
	}

	tmp = valuem + value * inputColor2[2];
	if (tmp <= 0.0f)
		output[2] = 0.0f;
	else {
		tmp = 1.0f - (1.0f - inputColor1[2]) / tmp;
		if (tmp < 0.0f)
			output[2] = 0.0f;
		else if (tmp > 1.0f)
			output[2] = 1.0f;
		else
			output[2] = tmp;
	}

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Color Operation ******** */

MixColorOperation::MixColorOperation() : MixBaseOperation()
{
	/* pass */
}

void MixColorOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	float colH, colS, colV;
	rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
	if (colS != 0.0f) {
		float rH, rS, rV;
		float tmpr, tmpg, tmpb;
		rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
		hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
		output[0] = (valuem * inputColor1[0]) + (value * tmpr);
		output[1] = (valuem * inputColor1[1]) + (value * tmpg);
		output[2] = (valuem * inputColor1[2]) + (value * tmpb);
	}
	else {
		copy_v3_v3(output, inputColor1);
	}
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Darken Operation ******** */

MixDarkenOperation::MixDarkenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDarkenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	output[0] = min_ff(inputColor1[0], inputColor2[0]) * value + inputColor1[0] * valuem;
	output[1] = min_ff(inputColor1[1], inputColor2[1]) * value + inputColor1[1] * valuem;
	output[2] = min_ff(inputColor1[2], inputColor2[2]) * value + inputColor1[2] * valuem;
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Difference Operation ******** */

MixDifferenceOperation::MixDifferenceOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDifferenceOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	output[0] = valuem * inputColor1[0] + value *fabsf(inputColor1[0] - inputColor2[0]);
	output[1] = valuem * inputColor1[1] + value *fabsf(inputColor1[1] - inputColor2[1]);
	output[2] = valuem * inputColor1[2] + value *fabsf(inputColor1[2] - inputColor2[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Difference Operation ******** */

MixDivideOperation::MixDivideOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDivideOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	if (inputColor2[0] != 0.0f)
		output[0] = valuem * (inputColor1[0]) + value * (inputColor1[0]) / inputColor2[0];
	else
		output[0] = 0.0f;
	if (inputColor2[1] != 0.0f)
		output[1] = valuem * (inputColor1[1]) + value * (inputColor1[1]) / inputColor2[1];
	else
		output[1] = 0.0f;
	if (inputColor2[2] != 0.0f)
		output[2] = valuem * (inputColor1[2]) + value * (inputColor1[2]) / inputColor2[2];
	else
		output[2] = 0.0f;

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Dodge Operation ******** */

MixDodgeOperation::MixDodgeOperation() : MixBaseOperation()
{
	/* pass */
}

void MixDodgeOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float tmp;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	if (inputColor1[0] != 0.0f) {
		tmp = 1.0f - value * inputColor2[0];
		if (tmp <= 0.0f)
			output[0] = 1.0f;
		else {
			tmp = inputColor1[0] / tmp;
			if (tmp > 1.0f)
				output[0] = 1.0f;
			else
				output[0] = tmp;
		}
	}
	else
		output[0] = 0.0f;

	if (inputColor1[1] != 0.0f) {
		tmp = 1.0f - value * inputColor2[1];
		if (tmp <= 0.0f)
			output[1] = 1.0f;
		else {
			tmp = inputColor1[1] / tmp;
			if (tmp > 1.0f)
				output[1] = 1.0f;
			else
				output[1] = tmp;
		}
	}
	else
		output[1] = 0.0f;

	if (inputColor1[2] != 0.0f) {
		tmp = 1.0f - value * inputColor2[2];
		if (tmp <= 0.0f)
			output[2] = 1.0f;
		else {
			tmp = inputColor1[2] / tmp;
			if (tmp > 1.0f)
				output[2] = 1.0f;
			else
				output[2] = tmp;
		}
	}
	else
		output[2] = 0.0f;

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Glare Operation ******** */

MixGlareOperation::MixGlareOperation() : MixBaseOperation()
{
	/* pass */
}

void MixGlareOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	float value;

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);
	value = inputValue[0];
	float mf = 2.f - 2.f * fabsf(value - 0.5f);

	if (inputColor1[0] < 0.0f) inputColor1[0] = 0.0f;
	if (inputColor1[1] < 0.0f) inputColor1[1] = 0.0f;
	if (inputColor1[2] < 0.0f) inputColor1[2] = 0.0f;

	output[0] = mf * max(inputColor1[0] + value * (inputColor2[0] - inputColor1[0]), 0.0f);
	output[1] = mf * max(inputColor1[1] + value * (inputColor2[1] - inputColor1[1]), 0.0f);
	output[2] = mf * max(inputColor1[2] + value * (inputColor2[2] - inputColor1[2]), 0.0f);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Hue Operation ******** */

MixHueOperation::MixHueOperation() : MixBaseOperation()
{
	/* pass */
}

void MixHueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	float colH, colS, colV;
	rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
	if (colS != 0.0f) {
		float rH, rS, rV;
		float tmpr, tmpg, tmpb;
		rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
		hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
		output[0] = valuem * (inputColor1[0]) + value * tmpr;
		output[1] = valuem * (inputColor1[1]) + value * tmpg;
		output[2] = valuem * (inputColor1[2]) + value * tmpb;
	}
	else {
		copy_v3_v3(output, inputColor1);
	}
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Lighten Operation ******** */

MixLightenOperation::MixLightenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixLightenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float tmp;
	tmp = value * inputColor2[0];
	if (tmp > inputColor1[0]) output[0] = tmp;
	else output[0] = inputColor1[0];
	tmp = value * inputColor2[1];
	if (tmp > inputColor1[1]) output[1] = tmp;
	else output[1] = inputColor1[1];
	tmp = value * inputColor2[2];
	if (tmp > inputColor1[2]) output[2] = tmp;
	else output[2] = inputColor1[2];
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Linear Light Operation ******** */

MixLinearLightOperation::MixLinearLightOperation() : MixBaseOperation()
{
	/* pass */
}

void MixLinearLightOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	if (inputColor2[0] > 0.5f)
		output[0] = inputColor1[0] + value * (2.0f * (inputColor2[0] - 0.5f));
	else
		output[0] = inputColor1[0] + value * (2.0f * (inputColor2[0]) - 1.0f);
	if (inputColor2[1] > 0.5f)
		output[1] = inputColor1[1] + value * (2.0f * (inputColor2[1] - 0.5f));
	else
		output[1] = inputColor1[1] + value * (2.0f * (inputColor2[1]) - 1.0f);
	if (inputColor2[2] > 0.5f)
		output[2] = inputColor1[2] + value * (2.0f * (inputColor2[2] - 0.5f));
	else
		output[2] = inputColor1[2] + value * (2.0f * (inputColor2[2]) - 1.0f);

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Multiply Operation ******** */

MixMultiplyOperation::MixMultiplyOperation() : MixBaseOperation()
{
	/* pass */
}

void MixMultiplyOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	output[0] = inputColor1[0] * (valuem + value * inputColor2[0]);
	output[1] = inputColor1[1] * (valuem + value * inputColor2[1]);
	output[2] = inputColor1[2] * (valuem + value * inputColor2[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Ovelray Operation ******** */

MixOverlayOperation::MixOverlayOperation() : MixBaseOperation()
{
	/* pass */
}

void MixOverlayOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}

	float valuem = 1.0f - value;

	if (inputColor1[0] < 0.5f) {
		output[0] = inputColor1[0] * (valuem + 2.0f * value * inputColor2[0]);
	}
	else {
		output[0] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[0])) * (1.0f - inputColor1[0]);
	}
	if (inputColor1[1] < 0.5f) {
		output[1] = inputColor1[1] * (valuem + 2.0f * value * inputColor2[1]);
	}
	else {
		output[1] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[1])) * (1.0f - inputColor1[1]);
	}
	if (inputColor1[2] < 0.5f) {
		output[2] = inputColor1[2] * (valuem + 2.0f * value * inputColor2[2]);
	}
	else {
		output[2] = 1.0f - (valuem + 2.0f * value * (1.0f - inputColor2[2])) * (1.0f - inputColor1[2]);
	}
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Saturation Operation ******** */

MixSaturationOperation::MixSaturationOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSaturationOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	float rH, rS, rV;
	rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
	if (rS != 0.0f) {
		float colH, colS, colV;
		rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
		hsv_to_rgb(rH, (valuem * rS + value * colS), rV, &output[0], &output[1], &output[2]);
	}
	else {
		copy_v3_v3(output, inputColor1);
	}

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Screen Operation ******** */

MixScreenOperation::MixScreenOperation() : MixBaseOperation()
{
	/* pass */
}

void MixScreenOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	output[0] = 1.0f - (valuem + value * (1.0f - inputColor2[0])) * (1.0f - inputColor1[0]);
	output[1] = 1.0f - (valuem + value * (1.0f - inputColor2[1])) * (1.0f - inputColor1[1]);
	output[2] = 1.0f - (valuem + value * (1.0f - inputColor2[2])) * (1.0f - inputColor1[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Soft Light Operation ******** */

MixSoftLightOperation::MixSoftLightOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSoftLightOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler) \
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	float scr, scg, scb;

	/* first calculate non-fac based Screen mix */
	scr = 1.0f - (1.0f - inputColor2[0]) * (1.0f - inputColor1[0]);
	scg = 1.0f - (1.0f - inputColor2[1]) * (1.0f - inputColor1[1]);
	scb = 1.0f - (1.0f - inputColor2[2]) * (1.0f - inputColor1[2]);

	output[0] = valuem * (inputColor1[0]) + value * (((1.0f - inputColor1[0]) * inputColor2[0] * (inputColor1[0])) + (inputColor1[0] * scr));
	output[1] = valuem * (inputColor1[1]) + value * (((1.0f - inputColor1[1]) * inputColor2[1] * (inputColor1[1])) + (inputColor1[1] * scg));
	output[2] = valuem * (inputColor1[2]) + value * (((1.0f - inputColor1[2]) * inputColor2[2] * (inputColor1[2])) + (inputColor1[2] * scb));
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Subtract Operation ******** */

MixSubtractOperation::MixSubtractOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSubtractOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue,   x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1,   x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2,   x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	output[0] = inputColor1[0] - value * (inputColor2[0]);
	output[1] = inputColor1[1] - value * (inputColor2[1]);
	output[2] = inputColor1[2] - value * (inputColor2[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}

/* ******** Mix Value Operation ******** */

MixValueOperation::MixValueOperation() : MixBaseOperation()
{
	/* pass */
}

void MixValueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->readSampled(inputValue, x, y, sampler);
	this->m_inputColor1Operation->readSampled(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->readSampled(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;

	float rH, rS, rV;
	float colH, colS, colV;
	rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
	rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
	hsv_to_rgb(rH, rS, (valuem * rV + value * colV), &output[0], &output[1], &output[2]);
	output[3] = inputColor1[3];

	clampIfNeeded(output);
}
