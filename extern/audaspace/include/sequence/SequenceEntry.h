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
 * @file SequenceEntry.h
 * @ingroup sequence
 * The SequenceEntry class.
 */

#include "sequence/AnimateableProperty.h"
#include "util/ILockable.h"

#include <mutex>
#include <memory>

AUD_NAMESPACE_BEGIN

class ISound;

/**
 * This class represents a sequenced entry in a sequencer sound.
 */
class AUD_API SequenceEntry : public ILockable
{
	friend class SequenceHandle;
private:
	/// The status of the entry. Changes every time a non-animated parameter changes.
	int m_status;

	/// The positional status of the entry. Changes every time the entry is moved.
	int m_pos_status;

	/// The sound status, changed when the sound is changed.
	int m_sound_status;

	/// The unique (regarding the sound) ID of the entry.
	int m_id;

	/// The sound this entry plays.
	std::shared_ptr<ISound> m_sound;

	/// The begin time.
	float m_begin;

	/// The end time.
	float m_end;

	/// How many seconds are skipped at the beginning.
	float m_skip;

	/// Whether the entry is muted.
	bool m_muted;

	/// Whether the position to the listener is relative or absolute
	bool m_relative;

	/// Maximum volume.
	float m_volume_max;

	/// Minimum volume.
	float m_volume_min;

	/// Maximum distance.
	float m_distance_max;

	/// Reference distance;
	float m_distance_reference;

	/// Attenuation
	float m_attenuation;

	/// Cone outer angle.
	float m_cone_angle_outer;

	/// Cone inner angle.
	float m_cone_angle_inner;

	/// Cone outer volume.
	float m_cone_volume_outer;

	/// The mutex for locking.
	std::recursive_mutex m_mutex;

	/// The animated volume.
	AnimateableProperty m_volume;

	/// The animated panning.
	AnimateableProperty m_panning;

	/// The animated pitch.
	AnimateableProperty m_pitch;

	/// The animated location.
	AnimateableProperty m_location;

	/// The animated orientation.
	AnimateableProperty m_orientation;

	// delete copy constructor and operator=
	SequenceEntry(const SequenceEntry&) = delete;
	SequenceEntry& operator=(const SequenceEntry&) = delete;

public:
	/**
	 * Creates a new sequenced entry.
	 * \param sound The sound this entry should play.
	 * \param begin The start time.
	 * \param end The end time or a negative value if determined by the sound.
	 * \param skip How much seconds should be skipped at the beginning.
	 * \param id The ID of the entry.
	 */
	SequenceEntry(std::shared_ptr<ISound> sound, float begin, float end, float skip, int id);
	virtual ~SequenceEntry();

	/**
	 * Locks the entry.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked entry.
	 */
	virtual void unlock();

	/**
	 * Retrieves the sound of the entry.
	 * \return The sound.
	 */
	std::shared_ptr<ISound> getSound();

	/**
	 * Sets the sound of the entry.
	 * \param sound The new sound.
	 */
	void setSound(std::shared_ptr<ISound> sound);

	/**
	 * Moves the entry.
	 * \param begin The new start time.
	 * \param end The new end time or a negative value if unknown.
	 * \param skip How many seconds to skip at the beginning.
	 */
	void move(float begin, float end, float skip);

	/**
	 * Retrieves the muting state of the entry.
	 * \return Whether the entry should is muted or not.
	 */
	bool isMuted();

	/**
	 * Sets the muting state of the entry.
	 * \param mute Whether the entry should be muted or not.
	 */
	void mute(bool mute);

	/**
	 * Retrieves the ID of the entry.
	 * \return The ID of the entry.
	 */
	int getID() const;

	/**
	 * Retrieves one of the animated properties of the entry.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, valid as long as the
	 *         entry is.
	 */
	AnimateableProperty* getAnimProperty(AnimateablePropertyType type);

	/**
	 * Checks whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \return Whether the source is relative.
	 */
	bool isRelative();

	/**
	 * Sets whether the source location, velocity and orientation are relative
	 * to the listener.
	 * \param relative Whether the source is relative.
	 * \return Whether the action succeeded.
	 */
	void setRelative(bool relative);

	/**
	 * Retrieves the maximum volume of a source.
	 * \return The maximum volume.
	 */
	float getVolumeMaximum();

	/**
	 * Sets the maximum volume of a source.
	 * \param volume The new maximum volume.
	 * \return Whether the action succeeded.
	 */
	void setVolumeMaximum(float volume);

	/**
	 * Retrieves the minimum volume of a source.
	 * \return The minimum volume.
	 */
	float getVolumeMinimum();

	/**
	 * Sets the minimum volume of a source.
	 * \param volume The new minimum volume.
	 * \return Whether the action succeeded.
	 */
	void setVolumeMinimum(float volume);

	/**
	 * Retrieves the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \return The maximum distance.
	 */
	float getDistanceMaximum();

	/**
	 * Sets the maximum distance of a source.
	 * If a source is further away from the reader than this distance, the
	 * volume will automatically be set to 0.
	 * \param distance The new maximum distance.
	 * \return Whether the action succeeded.
	 */
	void setDistanceMaximum(float distance);

	/**
	 * Retrieves the reference distance of a source.
	 * \return The reference distance.
	 */
	float getDistanceReference();

	/**
	 * Sets the reference distance of a source.
	 * \param distance The new reference distance.
	 * \return Whether the action succeeded.
	 */
	void setDistanceReference(float distance);

	/**
	 * Retrieves the attenuation of a source.
	 * \return The attenuation.
	 */
	float getAttenuation();

	/**
	 * Sets the attenuation of a source.
	 * This value is used for distance calculation.
	 * \param factor The new attenuation.
	 * \return Whether the action succeeded.
	 */
	void setAttenuation(float factor);

	/**
	 * Retrieves the outer angle of the cone of a source.
	 * \return The outer angle of the cone.
	 */
	float getConeAngleOuter();

	/**
	 * Sets the outer angle of the cone of a source.
	 * \param angle The new outer angle of the cone.
	 * \return Whether the action succeeded.
	 */
	void setConeAngleOuter(float angle);

	/**
	 * Retrieves the inner angle of the cone of a source.
	 * \return The inner angle of the cone.
	 */
	float getConeAngleInner();

	/**
	 * Sets the inner angle of the cone of a source.
	 * \param angle The new inner angle of the cone.
	 * \return Whether the action succeeded.
	 */
	void setConeAngleInner(float angle);

	/**
	 * Retrieves the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \return The outer volume of the cone.
	 */
	float getConeVolumeOuter();

	/**
	 * Sets the outer volume of the cone of a source.
	 * The volume between inner and outer angle is interpolated between inner
	 * volume and this value.
	 * \param volume The new outer volume of the cone.
	 * \return Whether the action succeeded.
	 */
	void setConeVolumeOuter(float volume);
};

AUD_NAMESPACE_END
