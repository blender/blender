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

/** \file audaspace/intern/AUD_SoftwareDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef AUD_SOFTWAREDEVICE
#define AUD_SOFTWAREDEVICE

#include "AUD_IDevice.h"
#include "AUD_IHandle.h"
#include "AUD_Mixer.h"
#include "AUD_Buffer.h"

#include <list>
#include <pthread.h>

/**
 * This device plays is a generic device with software mixing.
 * Classes implementing this have to:
 *  - Implement the playing function.
 *  - Prepare the m_specs, m_mixer variables.
 *  - Call the create and destroy functions.
 *  - Call the mix function to retrieve their audio data.
 */
class AUD_SoftwareDevice : public AUD_IDevice
{
protected:
	/// Saves the data for playback.
	class AUD_SoftwareHandle : public AUD_IHandle
	{
	public:
		/// The reader source.
		AUD_Reference<AUD_IReader> m_reader;

		/// Whether to keep the source if end of it is reached.
		bool m_keep;

		/// The volume of the source.
		float m_volume;

		/// The loop count of the source.
		int m_loopcount;

		/// The stop callback.
		stopCallback m_stop;

		/// Stop callback data.
		void* m_stop_data;

		/// Current status of the handle
		AUD_Status m_status;

		/// Own device.
		AUD_SoftwareDevice* m_device;

	public:

		AUD_SoftwareHandle(AUD_SoftwareDevice* device, AUD_Reference<AUD_IReader> reader, bool keep);

		virtual ~AUD_SoftwareHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(float position);
		virtual float getPosition();
		virtual AUD_Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);
	};

	/**
	 * The specification of the device.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The mixer.
	 */
	AUD_Reference<AUD_Mixer> m_mixer;

	/**
	 * Initializes member variables.
	 */
	void create();

	/**
	 * Uninitializes member variables.
	 */
	void destroy();

	/**
	 * Mixes the next samples into the buffer.
	 * \param buffer The target buffer.
	 * \param length The length in samples to be filled.
	 */
	void mix(data_t* buffer, int length);

	/**
	 * This function tells the device, to start or pause playback.
	 * \param playing True if device should playback.
	 */
	virtual void playing(bool playing)=0;

private:
	/**
	 * The reading buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The list of sounds that are currently playing.
	 */
	std::list<AUD_Reference<AUD_SoftwareHandle> > m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<AUD_Reference<AUD_SoftwareHandle> > m_pausedSounds;

	/**
	 * Whether there is currently playback.
	 */
	bool m_playback;

	/**
	 * The mutex for locking.
	 */
	pthread_mutex_t m_mutex;

	/**
	 * The overall volume of the device.
	 */
	float m_volume;

public:
	virtual AUD_DeviceSpecs getSpecs() const;
	virtual AUD_Reference<AUD_IHandle> play(AUD_Reference<AUD_IReader> reader, bool keep = false);
	virtual AUD_Reference<AUD_IHandle> play(AUD_Reference<AUD_IFactory> factory, bool keep = false);
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
};

#endif //AUD_SOFTWAREDEVICE
