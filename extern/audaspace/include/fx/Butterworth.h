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
 * @file Butterworth.h
 * @ingroup fx
 * The Butterworth class.
 */

#include "fx/DynamicIIRFilter.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound creates a butterworth lowpass filter reader.
 */
class AUD_API Butterworth : public DynamicIIRFilter
{
private:
	// delete copy constructor and operator=
	Butterworth(const Butterworth&) = delete;
	Butterworth& operator=(const Butterworth&) = delete;

public:
	/**
	 * Creates a new butterworth sound.
	 * \param sound The input sound.
	 * \param frequency The cutoff frequency.
	 */
	Butterworth(std::shared_ptr<ISound> sound, float frequency);
};

AUD_NAMESPACE_END
