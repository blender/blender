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

/** \file audaspace/intern/AUD_SequencerFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef AUD_SEQUENCERFACTORY
#define AUD_SEQUENCERFACTORY

#include "AUD_IFactory.h"
#include "AUD_AnimateableProperty.h"

#include <list>
#include <pthread.h>

class AUD_SequencerEntry;

/**
 * This factory creates a resampling reader that does simple linear resampling.
 */
class AUD_SequencerFactory : public AUD_IFactory
{
	friend class AUD_SequencerReader;
private:
	/**
	 * The target specification.
	 */
	AUD_Specs m_specs;

	int m_status;
	int m_entry_status;
	int m_id;
	std::list<AUD_Reference<AUD_SequencerEntry> > m_entries;
	bool m_muted;

	float m_fps;

	float m_speed_of_sound;
	float m_doppler_factor;
	AUD_DistanceModel m_distance_model;

	AUD_AnimateableProperty m_volume;
	AUD_AnimateableProperty m_location;
	AUD_AnimateableProperty m_orientation;

	/// The mutex for locking.
	pthread_mutex_t m_mutex;

	// hide copy constructor and operator=
	AUD_SequencerFactory(const AUD_SequencerFactory&);
	AUD_SequencerFactory& operator=(const AUD_SequencerFactory&);

public:
	AUD_SequencerFactory(AUD_Specs specs, float fps, bool muted);
	~AUD_SequencerFactory();

	/**
	 * Locks the factory.
	 */
	void lock();

	/**
	 * Unlocks the previously locked factory.
	 */
	void unlock();

	void setSpecs(AUD_Specs specs);
	void setFPS(float fps);

	void mute(bool muted);
	bool getMute() const;

	void setSpeedOfSound(float speed);
	float getSpeedOfSound() const;

	void setDopplerFactor(float factor);
	float getDopplerFactor() const;

	void setDistanceModel(AUD_DistanceModel model);
	AUD_DistanceModel getDistanceModel() const;

	AUD_AnimateableProperty* getAnimProperty(AUD_AnimateablePropertyType type);

	AUD_Reference<AUD_SequencerEntry> add(AUD_Reference<AUD_IFactory> sound, float begin, float end, float skip);
	void remove(AUD_Reference<AUD_SequencerEntry> entry);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //AUD_SEQUENCERFACTORY
