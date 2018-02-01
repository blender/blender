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
 * @file ISound.h
 * @ingroup general
 * The ISound interface.
 */

#include "Audaspace.h"

#include <memory>

AUD_NAMESPACE_BEGIN

class IReader;

/**
 * @interface ISound
 * This class represents a type of sound source and saves the necessary values
 * for it. It is able to create a reader that is actually usable for playback
 * of the respective sound source through the factory method createReader.
 */
class AUD_API ISound
{
public:
	/**
	 * Destroys the sound.
	 */
	virtual ~ISound() {}

	/**
	 * Creates a reader for playback of the sound source.
	 * \return A pointer to an IReader object or nullptr if there has been an
	 *         error.
	 * \exception Exception An exception may be thrown if there has been
	 *            a more unexpected error during reader creation.
	 */
	virtual std::shared_ptr<IReader> createReader()=0;
};

AUD_NAMESPACE_END
