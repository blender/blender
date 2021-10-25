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

/** \file audaspace/intern/AUD_Sequencer.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SEQUENCER_H__
#define __AUD_SEQUENCER_H__

#include "AUD_AnimateableProperty.h"
#include "AUD_IFactory.h"
#include "AUD_ILockable.h"

#include <list>
#include <pthread.h>

class AUD_SequencerEntry;

/**
 * This class represents sequenced entries to play a sound scene.
 */
class AUD_Sequencer : public AUD_ILockable
{
	friend class AUD_SequencerReader;
private:
	/// The target specification.
	AUD_Specs m_specs;

	/// The status of the sequence. Changes every time a non-animated parameter changes.
	int m_status;

	/// The entry status. Changes every time an entry is removed or added.
	int m_entry_status;

	/// The next unused ID for the entries.
	int m_id;

	/// The sequenced entries.
	std::list<boost::shared_ptr<AUD_SequencerEntry> > m_entries;

	/// Whether the whole scene is muted.
	bool m_muted;

	/// The FPS of the scene.
	float m_fps;

	/// Speed of Sound.
	float m_speed_of_sound;

	/// Doppler factor.
	float m_doppler_factor;

	/// Distance model.
	AUD_DistanceModel m_distance_model;

	/// The animated volume.
	AUD_AnimateableProperty m_volume;

	/// The animated listener location.
	AUD_AnimateableProperty m_location;

	/// The animated listener orientation.
	AUD_AnimateableProperty m_orientation;

	/// The mutex for locking.
	pthread_mutex_t m_mutex;

	// hide copy constructor and operator=
	AUD_Sequencer(const AUD_Sequencer&);
	AUD_Sequencer& operator=(const AUD_Sequencer&);

public:
	/**
	 * Creates a new sound scene.
	 * \param specs The output audio data specification.
	 * \param fps The FPS of the scene.
	 * \param muted Whether the whole scene is muted.
	 */
	AUD_Sequencer(AUD_Specs specs, float fps, bool muted);
	virtual ~AUD_Sequencer();

	/**
	 * Locks the sequence.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked sequence.
	 */
	virtual void unlock();

	/**
	 * Sets the audio output specification.
	 * \param specs The new specification.
	 */
	void setSpecs(AUD_Specs specs);

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
	bool getMute() const;

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
	AUD_DistanceModel getDistanceModel() const;

	/**
	 * Sets the distance model.
	 * \param model distance model.
	 */
	void setDistanceModel(AUD_DistanceModel model);

	/**
	 * Retrieves one of the animated properties of the sequence.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, valid as long as the
	 *         sequence is.
	 */
	AUD_AnimateableProperty* getAnimProperty(AUD_AnimateablePropertyType type);

	/**
	 * Adds a new entry to the scene.
	 * \param sound The sound this entry should play.
	 * \param begin The start time.
	 * \param end The end time or a negative value if determined by the sound.
	 * \param skip How much seconds should be skipped at the beginning.
	 * \return The entry added.
	 */
	boost::shared_ptr<AUD_SequencerEntry> add(boost::shared_ptr<AUD_IFactory> sound, float begin, float end, float skip);

	/**
	 * Removes an entry from the scene.
	 * \param entry The entry to remove.
	 */
	void remove(boost::shared_ptr<AUD_SequencerEntry> entry);
};

#endif //__AUD_SEQUENCER_H__
