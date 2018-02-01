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
 * @file IDynamicIIRFilterCalculator.h
 * @ingroup fx
 * The IDynamicIIRFilterCalculator interface.
 */

#include "respec/Specification.h"

#include <vector>

AUD_NAMESPACE_BEGIN

/**
 * @interface IDynamicIIRFilterCalculator
 * This interface calculates dynamic filter coefficients which depend on the
 * sampling rate for DynamicIIRFilterReaders.
 */
class AUD_API IDynamicIIRFilterCalculator
{
public:
	virtual ~IDynamicIIRFilterCalculator() {}

	/**
	 * Recalculates the filter coefficients.
	 * \param rate The sample rate of the audio data.
	 * \param[out] b The input filter coefficients.
	 * \param[out] a The output filter coefficients.
	 */
	virtual void recalculateCoefficients(SampleRate rate, std::vector<float>& b, std::vector<float>& a)=0;
};

AUD_NAMESPACE_END
