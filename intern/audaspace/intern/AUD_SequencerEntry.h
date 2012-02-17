/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_SequencerEntry.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SEQUENCERENTRY_H__
#define __AUD_SEQUENCERENTRY_H__

#include "AUD_Reference.h"
#include "AUD_AnimateableProperty.h"
#include "AUD_IFactory.h"

#include <pthread.h>

/**
 * This class represents a sequenced entry in a sequencer factory.
 */
class AUD_SequencerEntry
{
	friend class AUD_SequencerHandle;
private:
	/// The status of the entry. Changes every time a non-animated parameter changes.
	int m_status;

	/// The positional status of the entry. Changes every time the entry is moved.
	int m_pos_status;

	/// The sound status, changed when the sound is changed.
	int m_sound_status;

	/// The unique (regarding the factory) ID of the entry.
	int m_id;

	/// The sound this entry plays.
	AUD_Reference<AUD_IFactory> m_sound;

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
	pthread_mutex_t m_mutex;

	/// The animated volume.
	AUD_AnimateableProperty m_volume;

	/// The animated panning.
	AUD_AnimateableProperty m_panning;

	/// The animated pitch.
	AUD_AnimateableProperty m_pitch;

	/// The animated location.
	AUD_AnimateableProperty m_location;

	/// The animated orientation.
	AUD_AnimateableProperty m_orientation;

public:
	/**
	 * Creates a new sequenced entry.
	 * \param sound The sound this entry should play.
	 * \param begin The start time.
	 * \param end The end time or a negative value if determined by the sound.
	 * \param skip How much seconds should be skipped at the beginning.
	 * \param id The ID of the entry.
	 */
	AUD_SequencerEntry(AUD_Reference<AUD_IFactory> sound, float begin, float end, float skip, int id);
	virtual ~AUD_SequencerEntry();

	/**
	 * Locks the entry.
	 */
	void lock();

	/**
	 * Unlocks the previously locked entry.
	 */
	void unlock();

	/**
	 * Sets the sound of the entry.
	 * \param sound The new sound.
	 */
	void setSound(AUD_Reference<AUD_IFactory> sound);

	/**
	 * Moves the entry.
	 * \param begin The new start time.
	 * \param end The new end time or a negative value if unknown.
	 * \param skip How many seconds to skip at the beginning.
	 */
	void move(float begin, float end, float skip);

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
	AUD_AnimateableProperty* getAnimProperty(AUD_AnimateablePropertyType type);

	/**
	 * Updates all non-animated parameters of the entry.
	 * \param volume_max The maximum volume.
	 * \param volume_min The minimum volume.
	 * \param distance_max The maximum distance.
	 * \param distance_reference The reference distance.
	 * \param attenuation The attenuation.
	 * \param cone_angle_outer The outer cone opening angle.
	 * \param cone_angle_inner The inner cone opening angle.
	 * \param cone_volume_outer The volume outside the outer cone.
	 */
	void updateAll(float volume_max, float volume_min, float distance_max,
				   float distance_reference, float attenuation, float cone_angle_outer,
				   float cone_angle_inner, float cone_volume_outer);

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

#endif //__AUD_SEQUENCERENTRY_H__
