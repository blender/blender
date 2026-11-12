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
 * @file SequenceData.h
 * @ingroup sequence
 * The SequenceData class.
 */

#include "respec/Specification.h"
#include "sequence/AnimateableProperty.h"
#include "devices/I3DDevice.h"
#include "util/ILockable.h"

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

AUD_NAMESPACE_BEGIN

class SequenceEntry;
class ISound;

/**
 * This class represents sequenced entries to play a sound scene.
 */
class AUD_API SequenceData : public ILockable
{
	friend class SequenceReader;
private:
	/// The target specification.
	Specs m_specs;

	/// The status of the sequence. Changes every time a non-animated parameter changes.
	int m_status;

	/// The entry status. Changes every time an entry is removed or added.
	int m_entry_status;

	/// The next unused ID for the entries.
	int m_id;

	/// The sequenced entries.
	std::list<std::shared_ptr<SequenceEntry> > m_entries;

	/// Whether the whole scene is muted.
	bool m_muted;

	/// The FPS of the scene.
	float m_fps;

	/// Speed of Sound.
	float m_speed_of_sound;

	/// Doppler factor.
	float m_doppler_factor;

	/// Distance model.
	DistanceModel m_distance_model;

	/// The animated volume.
	AnimateableProperty m_volume;

	/// The animated listener location.
	AnimateableProperty m_location;

	/// The animated listener orientation.
	AnimateableProperty m_orientation;

	/// The animated properties of a reference to the scene by entry ID, -1 for the scene, and type.
	typedef std::map<std::pair<int, AnimateablePropertyType>, std::unique_ptr<AnimateableProperty> > ReferenceProperties;

	/// The animated properties of references to the scene by reference, see getReferenceAnimProperty().
	std::map<std::vector<int>, ReferenceProperties> m_reference_properties;

	/// The mutex for locking.
	std::recursive_mutex m_mutex;

	// delete copy constructor and operator=
	SequenceData(const SequenceData&) = delete;
	SequenceData& operator=(const SequenceData&) = delete;

public:
	/**
	 * Creates a new sound scene.
	 * \param specs The output audio data specification.
	 * \param fps The FPS of the scene.
	 * \param muted Whether the whole scene is muted.
	 */
	SequenceData(Specs specs, float fps, bool muted);
	virtual ~SequenceData();

	/**
	 * Locks the sequence.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked sequence.
	 */
	virtual void unlock();

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
	 * Retrieves one of the animated properties of the sequence.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, valid as long as the
	 *         sequence is.
	 */
	AnimateableProperty* getAnimProperty(AnimateablePropertyType type);

	/**
	 * Adds a new entry to the scene.
	 * \param sound The sound this entry should play.
	 * \param sequence_data Reference to sequence_data. Mainly needed to get the FPS of the scene.
	 * \param begin The start time.
	 * \param end The end time or a negative value if determined by the sound.
	 * \param skip How much seconds should be skipped at the beginning.
	 * \return The entry added.
	 */
	std::shared_ptr<SequenceEntry> add(std::shared_ptr<ISound> sound, std::shared_ptr<SequenceData> sequence_data, double begin, double end, double skip);

	/**
	 * Removes an entry from the scene.
	 * \param entry The entry to remove.
	 */
	void remove(std::shared_ptr<SequenceEntry> entry);

	/**
	 * Retrieves an animated property that a reference to the scene reads instead of a property of
	 * the scene or of one of its entries. References are the entries of other scenes that play the
	 * scene. With these properties, references can have different animation. Readers use the
	 * volume of the scene and the volume and panning of its entries.
	 * \param reference The IDs of the entries from the scene that a device plays down to the entry
	 *        that plays this scene, see SequenceEntry::getID().
	 * \param entry The ID of the entry of the property, or -1 for a property of the scene.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, or nullptr if the reference doesn't have it. The
	 *         sequence owns the property: the pointer becomes invalid when
	 *         setReferenceAnimProperty() replaces the same property, when
	 *         clearReferenceAnimProperties() is called or when the sequence is destroyed. The
	 *         lookup locks the sequence only while it runs, so a caller that uses the pointer while
	 *         another thread can replace or clear the property must hold lock() for as long as it
	 *         uses the pointer, as readers do for a whole read.
	 */
	AnimateableProperty* getReferenceAnimProperty(const std::vector<int>& reference, int entry, AnimateablePropertyType type);

	/**
	 * Retrieves an animated property of a reference to the scene like getReferenceAnimProperty(),
	 * and adds it if the reference doesn't have it yet.
	 * \param reference The IDs of the entries from the scene that a device plays down to the entry
	 *        that plays this scene.
	 * \param entry The ID of the entry of the property, or -1 for a property of the scene.
	 * \param type Which animated property to retrieve.
	 * \param value The value of an added property.
	 * \return A pointer to the animated property, with the same lifetime and locking requirements
	 *         as the one of getReferenceAnimProperty().
	 */
	AnimateableProperty* addReferenceAnimProperty(const std::vector<int>& reference, int entry, AnimateablePropertyType type, float value);

	/**
	 * Replaces an animated property of a reference to the scene, see getReferenceAnimProperty().
	 * Readers read either the replaced or the new property during a read. The replaced property is
	 * destroyed, pointers to it become invalid.
	 * \param reference The IDs of the entries from the scene that a device plays down to the entry
	 *        that plays this scene.
	 * \param entry The ID of the entry of the property, or -1 for a property of the scene.
	 * \param type Which animated property to replace.
	 * \param property The new property.
	 */
	void setReferenceAnimProperty(const std::vector<int>& reference, int entry, AnimateablePropertyType type, std::unique_ptr<AnimateableProperty> property);

	/**
	 * Removes the animated properties of all references to the scene, which read the properties of
	 * the scene and its entries again.
	 */
	void clearReferenceAnimProperties();
};

AUD_NAMESPACE_END
