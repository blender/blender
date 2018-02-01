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
 * @file Loop.h
 * @ingroup fx
 * The Loop class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound loops another sound.
 * \note The reader has to be seekable.
 */
class AUD_API Loop : public Effect
{
private:
	/**
	 * The loop count.
	 */
	const int m_loop;

	// delete copy constructor and operator=
	Loop(const Loop&) = delete;
	Loop& operator=(const Loop&) = delete;

public:
	/**
	 * Creates a new loop sound.
	 * \param sound The input sound.
	 * \param loop The desired loop count, negative values result in endless
	 *        looping.
	 */
	Loop(std::shared_ptr<ISound> sound, int loop = -1);

	/**
	 * Returns the loop count.
	 */
	int getLoop() const;

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
