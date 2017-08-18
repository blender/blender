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
 * @file Reverse.h
 * @ingroup fx
 * The Reverse class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound reads another sound reverted.
 * \note Readers from the underlying sound must be seekable.
 */
class AUD_API Reverse : public Effect
{
private:
	// delete copy constructor and operator=
	Reverse(const Reverse&) = delete;
	Reverse& operator=(const Reverse&) = delete;

public:
	/**
	 * Creates a new reverse sound.
	 * \param sound The input sound.
	 */
	Reverse(std::shared_ptr<ISound> sound);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
