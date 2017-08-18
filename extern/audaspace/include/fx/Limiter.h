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
 * @file Limiter.h
 * @ingroup fx
 * The Limiter class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound limits another sound in start and end time.
 */
class AUD_API Limiter : public Effect
{
private:
	/**
	 * The start time.
	 */
	const float m_start;

	/**
	 * The end time.
	 */
	const float m_end;

	// delete copy constructor and operator=
	Limiter(const Limiter&) = delete;
	Limiter& operator=(const Limiter&) = delete;

public:
	/**
	 * Creates a new limiter sound.
	 * \param sound The input sound.
	 * \param start The desired start time.
	 * \param end The desired end time, a negative value signals that it should
	 *            play to the end.
	 */
	Limiter(std::shared_ptr<ISound> sound,
					   float start = 0, float end = -1);

	/**
	 * Returns the start time.
	 */
	float getStart() const;

	/**
	 * Returns the end time.
	 */
	float getEnd() const;

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
