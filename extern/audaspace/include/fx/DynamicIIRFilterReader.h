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
 * @file DynamicIIRFilterReader.h
 * @ingroup fx
 * The DynamicIIRFilterReader class.
 */

#include "fx/IIRFilterReader.h"

AUD_NAMESPACE_BEGIN

class IDynamicIIRFilterCalculator;

/**
 * This class is for dynamic infinite impulse response filters with simple
 * coefficients that change depending on the sample rate.
 */
class AUD_API DynamicIIRFilterReader : public IIRFilterReader
{
private:
	/**
	 * The sound for dynamically recalculating filter coefficients.
	 */
	std::shared_ptr<IDynamicIIRFilterCalculator> m_calculator;

public:
	/**
	 * Creates a new DynamicIIRFilterReader.
	 * @param reader The reader the filter is applied on.
	 * @param calculator The IDynamicIIRFilterCalculator that recalculates the filter coefficients.
	 */
	DynamicIIRFilterReader(std::shared_ptr<IReader> reader,
							   std::shared_ptr<IDynamicIIRFilterCalculator> calculator);

	/**
	 * The function sampleRateChanged is called whenever the sample rate of the
	 * underlying reader changes and thus updates the filter coefficients.
	 * @param rate The new sample rate.
	 */
	virtual void sampleRateChanged(SampleRate rate);
};

AUD_NAMESPACE_END
