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
 * @file Delay.h
 * @ingroup fx
 * The Delay class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound plays another sound delayed.
 */
class AUD_API Delay : public Effect
{
private:
	/**
	 * The delay in samples.
	 */
	const double m_delay;

	// delete copy constructor and operator=
	Delay(const Delay&) = delete;
	Delay& operator=(const Delay&) = delete;

public:
	/**
	 * Creates a new delay sound.
	 * \param sound The input sound.
	 * \param delay The desired delay in seconds.
	 */
	Delay(std::shared_ptr<ISound> sound, double delay = 0);

	/**
	 * Returns the delay in seconds.
	 */
	double getDelay() const;

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
