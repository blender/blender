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
 * @file Highpass.h
 * @ingroup fx
 * The Highpass class.
 */

#include "fx/DynamicIIRFilter.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound creates a highpass filter reader.
 */
class AUD_API Highpass : public DynamicIIRFilter
{
private:
	// delete copy constructor and operator=
	Highpass(const Highpass&) = delete;
	Highpass& operator=(const Highpass&) = delete;

public:
	/**
	 * Creates a new highpass sound.
	 * \param sound The input sound.
	 * \param frequency The cutoff frequency.
	 * \param Q The Q factor.
	 */
	Highpass(std::shared_ptr<ISound> sound, float frequency, float Q = 1.0f);
};

AUD_NAMESPACE_END
