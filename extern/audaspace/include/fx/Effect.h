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
 * @file Effect.h
 * @ingroup fx
 * The Effect class.
 */

#include "ISound.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound is a base class for all effect factories that take one other
 * sound as input.
 */
class AUD_API Effect : public ISound
{
private:
	// delete copy constructor and operator=
	Effect(const Effect&) = delete;
	Effect& operator=(const Effect&) = delete;

protected:
	/**
	 * If there is no reader it is created out of this sound.
	 */
	std::shared_ptr<ISound> m_sound;

	/**
	 * Returns the reader created out of the sound.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader created out of the sound.
	 */
	inline std::shared_ptr<IReader> getReader() const
	{
		return m_sound->createReader();
	}

public:
	/**
	 * Creates a new sound.
	 * \param sound The input sound.
	 */
	Effect(std::shared_ptr<ISound> sound);

	/**
	 * Destroys the sound.
	 */
	virtual ~Effect();

	/**
	 * Returns the saved sound.
	 * \return The sound or nullptr if there has no sound been saved.
	 */
	std::shared_ptr<ISound> getSound() const;
};

AUD_NAMESPACE_END
