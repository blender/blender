/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

/**
 * @file HighpassCalculator.h
 * @ingroup fx
 * The HighpassCalculator class.
 */

#include "fx/IDynamicIIRFilterCalculator.h"

AUD_NAMESPACE_BEGIN

/**
 * The HighpassCalculator class calculates high pass filter coefficients for a
 * dynamic DynamicIIRFilter.
 */
class AUD_LOCAL HighpassCalculator : public IDynamicIIRFilterCalculator
{
private:
	/**
	 * The cutoff frequency.
	 */
	const float m_frequency;

	/**
	 * The Q factor.
	 */
	const float m_Q;

	// delete copy constructor and operator=
	HighpassCalculator(const HighpassCalculator&) = delete;
	HighpassCalculator& operator=(const HighpassCalculator&) = delete;

public:
	/**
	 * Creates a HighpassCalculator object.
	 * @param frequency The cutoff frequency.
	 * @param Q The Q factor of the filter. If unsure, use 1.0 as default.
	 */
	HighpassCalculator(float frequency, float Q);

	virtual void recalculateCoefficients(SampleRate rate, std::vector<float> &b, std::vector<float> &a);
};

AUD_NAMESPACE_END
