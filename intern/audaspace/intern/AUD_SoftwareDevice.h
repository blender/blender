/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_SOFTWAREDEVICE
#define AUD_SOFTWAREDEVICE

#include "AUD_IDevice.h"
struct AUD_SoftwareHandle;
class AUD_Mixer;
class AUD_Buffer;

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
	/**
	 * The specification of the device.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The mixer.
	 */
	AUD_Mixer* m_mixer;

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
	 * The list of sounds that are currently playing.
	 */
	std::list<AUD_SoftwareHandle*> m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<AUD_SoftwareHandle*> m_pausedSounds;

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

	/**
	 * Checks if a handle is valid.
	 * \param handle The handle to check.
	 * \return Whether the handle is valid.
	 */
	bool isValid(AUD_Handle* handle);

public:
	virtual AUD_DeviceSpecs getSpecs() const;
	virtual AUD_Handle* play(AUD_IFactory* factory, bool keep = false);
	virtual bool pause(AUD_Handle* handle);
	virtual bool resume(AUD_Handle* handle);
	virtual bool stop(AUD_Handle* handle);
	virtual bool getKeep(AUD_Handle* handle);
	virtual bool setKeep(AUD_Handle* handle, bool keep);
	virtual bool seek(AUD_Handle* handle, float position);
	virtual float getPosition(AUD_Handle* handle);
	virtual AUD_Status getStatus(AUD_Handle* handle);
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
	virtual float getVolume(AUD_Handle* handle);
	virtual bool setVolume(AUD_Handle* handle, float volume);
	virtual float getPitch(AUD_Handle* handle);
	virtual bool setPitch(AUD_Handle* handle, float pitch);
	virtual int getLoopCount(AUD_Handle* handle);
	virtual bool setLoopCount(AUD_Handle* handle, int count);
	virtual bool setStopCallback(AUD_Handle* handle, stopCallback callback = NULL, void* data = NULL);
};

#endif //AUD_SOFTWAREDEVICE
