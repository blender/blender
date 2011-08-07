/*
 * $Id$
 *
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


#ifndef AUD_SEQUENCERENTRY
#define AUD_SEQUENCERENTRY

#include "AUD_Reference.h"
#include "AUD_AnimateableProperty.h"
#include "AUD_IFactory.h"

#include <pthread.h>

class AUD_SequencerEntry
{
	friend class AUD_SequencerHandle;
private:
	int m_status;
	int m_pos_status;
	int m_sound_status;
	int m_id;

	AUD_Reference<AUD_IFactory> m_sound;
	float m_begin;
	float m_end;
	float m_skip;
	bool m_muted;
	bool m_relative;
	float m_volume_max;
	float m_volume_min;
	float m_distance_max;
	float m_distance_reference;
	float m_attenuation;
	float m_cone_angle_outer;
	float m_cone_angle_inner;
	float m_cone_volume_outer;

	/// The mutex for locking.
	pthread_mutex_t m_mutex;

	AUD_AnimateableProperty m_volume;
	AUD_AnimateableProperty m_panning;
	AUD_AnimateableProperty m_pitch;
	AUD_AnimateableProperty m_location;
	AUD_AnimateableProperty m_orientation;

public:
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

	void setSound(AUD_Reference<AUD_IFactory> sound);

	void move(float begin, float end, float skip);
	void mute(bool mute);

	int getID() const;

	AUD_AnimateableProperty* getAnimProperty(AUD_AnimateablePropertyType type);

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

#endif //AUD_SEQUENCERENTRY
