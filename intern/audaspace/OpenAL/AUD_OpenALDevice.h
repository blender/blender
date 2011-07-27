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

/** \file audaspace/OpenAL/AUD_OpenALDevice.h
 *  \ingroup audopenal
 */


#ifndef AUD_OPENALDEVICE
#define AUD_OPENALDEVICE

#include "AUD_IDevice.h"
#include "AUD_I3DDevice.h"
struct AUD_OpenALHandle;
struct AUD_OpenALBufferedFactory;

#include <AL/al.h>
#include <AL/alc.h>
#include <list>
#include <pthread.h>

/**
 * This device plays through OpenAL.
 */
class AUD_OpenALDevice : public AUD_IDevice, public AUD_I3DDevice
{
private:
	/**
	 * The OpenAL device handle.
	 */
	ALCdevice* m_device;

	/**
	 * The OpenAL context.
	 */
	ALCcontext* m_context;

	/**
	 * The specification of the device.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * Whether the device has the AL_EXT_MCFORMATS extension.
	 */
	bool m_useMC;

	/**
	 * The list of sounds that are currently playing.
	 */
	std::list<AUD_OpenALHandle*>* m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<AUD_OpenALHandle*>* m_pausedSounds;

	/**
	 * The list of buffered factories.
	 */
	std::list<AUD_OpenALBufferedFactory*>* m_bufferedFactories;

	/**
	 * The mutex for locking.
	 */
	pthread_mutex_t m_mutex;

	/**
	 * The streaming thread.
	 */
	pthread_t m_thread;

	/**
	 * The condition for streaming thread wakeup.
	 */
	bool m_playing;

	/**
	 * Buffer size.
	 */
	int m_buffersize;

	/**
	 * Starts the streaming thread.
	 */
	void start(bool join = true);

	/**
	 * Checks if a handle is valid.
	 * \param handle The handle to check.
	 * \return Whether the handle is valid.
	 */
	bool isValid(AUD_Handle* handle);

	/**
	 * Gets the format according to the specs.
	 * \param format The variable to put the format into.
	 * \param specs The specs to read the channel count from.
	 * \return Whether the format is valid or not.
	 */
	bool getFormat(ALenum &format, AUD_Specs specs);

	// hide copy constructor and operator=
	AUD_OpenALDevice(const AUD_OpenALDevice&);
	AUD_OpenALDevice& operator=(const AUD_OpenALDevice&);

public:
	/**
	 * Opens the OpenAL audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \note The buffersize will be multiplicated by three for this device.
	 * \exception AUD_Exception Thrown if the audio device cannot be opened.
	 */
	AUD_OpenALDevice(AUD_DeviceSpecs specs,
					 int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Streaming thread main function.
	 */
	void updateStreams();

	virtual ~AUD_OpenALDevice();

	virtual AUD_DeviceSpecs getSpecs() const;
	virtual AUD_Handle* play(AUD_IReader* reader, bool keep = false);
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

	virtual AUD_Vector3 getListenerLocation() const;
	virtual void setListenerLocation(const AUD_Vector3& location);
	virtual AUD_Vector3 getListenerVelocity() const;
	virtual void setListenerVelocity(const AUD_Vector3& velocity);
	virtual AUD_Quaternion getListenerOrientation() const;
	virtual void setListenerOrientation(const AUD_Quaternion& orientation);
	virtual float getSpeedOfSound() const;
	virtual void setSpeedOfSound(float speed);
	virtual float getDopplerFactor() const;
	virtual void setDopplerFactor(float factor);
	virtual AUD_DistanceModel getDistanceModel() const;
	virtual void setDistanceModel(AUD_DistanceModel model);
	virtual AUD_Vector3 getSourceLocation(AUD_Handle* handle);
	virtual bool setSourceLocation(AUD_Handle* handle, const AUD_Vector3& location);
	virtual AUD_Vector3 getSourceVelocity(AUD_Handle* handle);
	virtual bool setSourceVelocity(AUD_Handle* handle, const AUD_Vector3& velocity);
	virtual AUD_Quaternion getSourceOrientation(AUD_Handle* handle);
	virtual bool setSourceOrientation(AUD_Handle* handle, const AUD_Quaternion& orientation);
	virtual bool isRelative(AUD_Handle* handle);
	virtual bool setRelative(AUD_Handle* handle, bool relative);
	virtual float getVolumeMaximum(AUD_Handle* handle);
	virtual bool setVolumeMaximum(AUD_Handle* handle, float volume);
	virtual float getVolumeMinimum(AUD_Handle* handle);
	virtual bool setVolumeMinimum(AUD_Handle* handle, float volume);
	virtual float getDistanceMaximum(AUD_Handle* handle);
	virtual bool setDistanceMaximum(AUD_Handle* handle, float distance);
	virtual float getDistanceReference(AUD_Handle* handle);
	virtual bool setDistanceReference(AUD_Handle* handle, float distance);
	virtual float getAttenuation(AUD_Handle* handle);
	virtual bool setAttenuation(AUD_Handle* handle, float factor);
	virtual float getConeAngleOuter(AUD_Handle* handle);
	virtual bool setConeAngleOuter(AUD_Handle* handle, float angle);
	virtual float getConeAngleInner(AUD_Handle* handle);
	virtual bool setConeAngleInner(AUD_Handle* handle, float angle);
	virtual float getConeVolumeOuter(AUD_Handle* handle);
	virtual bool setConeVolumeOuter(AUD_Handle* handle, float volume);
};

#endif //AUD_OPENALDEVICE
