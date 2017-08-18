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
 * @file DynamicIIRFilter.h
 * @ingroup fx
 * The DynamicIIRFilter class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

class IDynamicIIRFilterCalculator;

/**
 * This sound creates a IIR filter reader.
 *
 * This means that on sample rate change the filter recalculates its
 * coefficients.
 */
class AUD_API DynamicIIRFilter : public Effect
{
protected:
	/// The IDynamicIIRFilterCalculator that calculates the dynamic filter coefficients.
	std::shared_ptr<IDynamicIIRFilterCalculator> m_calculator;

public:
	/**
	 * Creates a new Dynmic IIR filter sound.
	 * \param sound The input sound.
	 * \param calculator The calculator which recalculates the dynamic filter coefficients.
	 */
	DynamicIIRFilter(std::shared_ptr<ISound> sound, std::shared_ptr<IDynamicIIRFilterCalculator> calculator);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
