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

/** \file audaspace/intern/AUD_SequencerFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SEQUENCERFACTORY_H__
#define __AUD_SEQUENCERFACTORY_H__

#include "AUD_IFactory.h"
#include "AUD_AnimateableProperty.h"
//#include "AUD_ILockable.h"
#include "AUD_Sequencer.h"

#include <list>
#include <pthread.h>

class AUD_SequencerEntry;

/**
 * This factory represents sequenced entries to play a sound scene.
 */
class AUD_SequencerFactory : public AUD_IFactory//, public AUD_ILockable
{
	friend class AUD_SequencerReader;
private:
	/// The sequence.
	boost::shared_ptr<AUD_Sequencer> m_sequence;

	// hide copy constructor and operator=
	AUD_SequencerFactory(const AUD_SequencerFactory&);
	AUD_SequencerFactory& operator=(const AUD_SequencerFactory&);

public:
	/**
	 * Creates a new sound scene.
	 * \param specs The output audio data specification.
	 * \param fps The FPS of the scene.
	 * \param muted Whether the whole scene is muted.
	 */
	AUD_SequencerFactory(AUD_Specs specs, float fps, bool muted);

#if 0
	/**
	 * Locks the factory.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked factory.
	 */
	virtual void unlock();
#endif

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
	 * Retrieves one of the animated properties of the factory.
	 * \param type Which animated property to retrieve.
	 * \return A pointer to the animated property, valid as long as the
	 *         factory is.
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

	/**
	 * Creates a new reader with high quality resampling.
	 * \return The new reader.
	 */
	boost::shared_ptr<AUD_IReader> createQualityReader();

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif //__AUD_SEQUENCERFACTORY_H__
