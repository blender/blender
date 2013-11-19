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

#include "COM_ColorBalanceLGGOperation.h"
#include "BLI_math.h"


inline float colorbalance_lgg(float in, float lift_lgg, float gamma_inv, float gain)
{
	/* 1:1 match with the sequencer with linear/srgb conversions, the conversion isnt pretty
	 * but best keep it this way, sice testing for durian shows a similar calculation
	 * without lin/srgb conversions gives bad results (over-saturated shadows) with colors
	 * slightly below 1.0. some correction can be done but it ends up looking bad for shadows or lighter tones - campbell */
	float x = (((linearrgb_to_srgb(in) - 1.0f) * lift_lgg) + 1.0f) * gain;

	/* prevent NaN */
	if (x < 0.f) x = 0.f;

	return powf(srgb_to_linearrgb(x), gamma_inv);
}

ColorBalanceLGGOperation::ColorBalanceLGGOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_inputValueOperation = NULL;
	this->m_inputColorOperation = NULL;
	this->setResolutionInputSocketIndex(1);
}

void ColorBalanceLGGOperation::initExecution()
{
	this->m_inputValueOperation = this->getInputSocketReader(0);
	this->m_inputColorOperation = this->getInputSocketReader(1);
}

void ColorBalanceLGGOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor[4];
	float value[4];
	
	this->m_inputValueOperation->readSampled(value, x, y, sampler);
	this->m_inputColorOperation->readSampled(inputColor, x, y, sampler);
	
	float fac = value[0];
	fac = min(1.0f, fac);
	const float mfac = 1.0f - fac;
	
	output[0] = mfac * inputColor[0] + fac * colorbalance_lgg(inputColor[0], this->m_lift[0], this->m_gamma_inv[0], this->m_gain[0]);
	output[1] = mfac * inputColor[1] + fac * colorbalance_lgg(inputColor[1], this->m_lift[1], this->m_gamma_inv[1], this->m_gain[1]);
	output[2] = mfac * inputColor[2] + fac * colorbalance_lgg(inputColor[2], this->m_lift[2], this->m_gamma_inv[2], this->m_gain[2]);
	output[3] = inputColor[3];

}

void ColorBalanceLGGOperation::deinitExecution()
{
	this->m_inputValueOperation = NULL;
	this->m_inputColorOperation = NULL;
}
