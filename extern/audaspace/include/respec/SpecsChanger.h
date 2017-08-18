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
 * @file SpecsChanger.h
 * @ingroup respec
 * The SpecsChanger class.
 */

#include "ISound.h"
#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound is a base class for all mixer factories.
 */
class AUD_API SpecsChanger : public ISound
{
protected:
	/**
	 * The target specification for resampling.
	 */
	const DeviceSpecs m_specs;

	/**
	 * If there is no reader it is created out of this sound.
	 */
	std::shared_ptr<ISound> m_sound;

	/**
	 * Returns the reader created out of the sound.
	 * This method can be used for the createReader function of the implementing
	 * classes.
	 * \return The reader to mix.
	 */
	std::shared_ptr<IReader> getReader() const;

public:
	/**
	 * Creates a new sound.
	 * \param sound The sound to create the readers to mix out of.
	 * \param specs The target specification.
	 */
	SpecsChanger(std::shared_ptr<ISound> sound, DeviceSpecs specs);

	/**
	 * Returns the target specification for resampling.
	 */
	DeviceSpecs getSpecs() const;

	/**
	 * Returns the saved sound.
	 * \return The sound.
	 */
	std::shared_ptr<ISound> getSound() const;
};

AUD_NAMESPACE_END
