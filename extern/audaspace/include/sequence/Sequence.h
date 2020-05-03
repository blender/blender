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
 * @file Sequence.h
 * @ingroup sequence
 * The Sequence class.
 */

#include "ISound.h"
#include "respec/Specification.h"
#include "devices/I3DDevice.h"
#include "sequence/AnimateableProperty.h"

#include <list>

AUD_NAMESPACE_BEGIN

class SequenceEntry;
class SequenceData;

/**
 * This sound represents sequenced entries to play a sound scene.
 */
class AUD_API Sequence : public ISound
{
	friend class SequenceReader;
private:
	/// The sequence.
	std::shared_ptr<SequenceData> m_sequence;

	// delete copy constructor and operator=
	Sequence(const Sequence&) = delete;
	Sequence& operator=(const Sequence&) = delete;

public:
	/**
	 * Creates a new sound scene.
	 * \param specs The output audio data specification.
	 * \param fps The FPS of the scene.
	 * \param muted Whether the whole scene is muted.
	 */
	Sequence(Specs specs, float fps, bool muted);

	/**
	 * Retrieves the audio output specification.
	 * \return The specification.
	 */
	Specs getSpecs();

	/**
	 * Sets the audio output specification.
	 * \param specs The new specification.
	 */
	void setSpecs(Specs specs);

	/**
	 * Retrieves the scene's FPS.
	 * \return The scene's FPS.
	 */
	float getFPS() const;

	/**
	 * Sets the scene's FPS.
	 * \param fps The new FPS.
	 */
	void setFPS(float fps);

	/**
	 * Sets the muting state of the scene.
	 * \param muted Whether the scene is muted.
	 */
	void mute(bool muted);

	/**
	 * Retrieves the muting state of the scene.
	 * \return Whether the scene is muted.
	 */
	bool isMuted() const;

	/**
	 * Retrieves the speed of sound.
	 * This value is needed for doppler effect calculation.
	 * \return The speed of sound.
	 */
	float getSpeedOfSound() const;

	/**
	 * Sets the speed of sound.
	 * This value is needed for doppler effect calculation.
	 * \param speed The new speed of sound.
	 */
	void setSpeedOfSound(float speed);

	/**
	 * Retrieves the doppler factor.
	 * This value is a scaling factor for the velocity vectors of sources and
	 * listener which is used while calculating the doppler effect.
	 * \return The doppler factor.
	 */
	float getDopplerFactor() const;

	/**
	 * Sets the doppler factor.
	 * This value is a scaling factor for the velocity vectors of sources and
	 * listener which is used while calculating the doppler effect.
	 * \param factor The new doppler factor.
	 */
	void setDopplerFactor(float factor);

	/**
	 * Retrieves the distance model.
	 * \return The distance model.
	 */
	DistanceModel getDistanceModel() const;

	/**
	 * Sets the distance model.
	 * \param model distance model.
	 */
	void setDistanceModel(DistanceModel model);

	/**
	 * Retrieves one of the animated properties of the sound.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, valid as long as the
	 *         sound is.
	 */
	AnimateableProperty* getAnimProperty(AnimateablePropertyType type);

	/**
	 * Adds a new entry to the scene.
	 * \param sound The sound this entry should play.
	 * \param begin The start time.
	 * \param end The end time or a negative value if determined by the sound.
	 * \param skip How much seconds should be skipped at the beginning.
	 * \return The entry added.
	 */
	std::shared_ptr<SequenceEntry> add(std::shared_ptr<ISound> sound, double begin, double end, double skip);

	/**
	 * Removes an entry from the scene.
	 * \param entry The entry to remove.
	 */
	void remove(std::shared_ptr<SequenceEntry> entry);

	/**
	 * Creates a new reader with high quality resampling.
	 * \return The new reader.
	 */
	std::shared_ptr<IReader> createQualityReader();

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
