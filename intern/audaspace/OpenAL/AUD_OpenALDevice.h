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

#ifndef AUD_OPENALDEVICE
#define AUD_OPENALDEVICE

#include "AUD_IDevice.h"
#include "AUD_I3DDevice.h"
struct AUD_OpenALHandle;
struct AUD_OpenALBufferedFactory;
class AUD_ConverterFactory;

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
	* The converter factory for readers with wrong input format.
	*/
	AUD_ConverterFactory* m_converter;

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
	void start();

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

	virtual AUD_DeviceSpecs getSpecs();
	virtual AUD_Handle* play(AUD_IFactory* factory, bool keep = false);
	virtual bool pause(AUD_Handle* handle);
	virtual bool resume(AUD_Handle* handle);
	virtual bool stop(AUD_Handle* handle);
	virtual bool setKeep(AUD_Handle* handle, bool keep);
	virtual bool sendMessage(AUD_Handle* handle, AUD_Message &message);
	virtual bool seek(AUD_Handle* handle, float position);
	virtual float getPosition(AUD_Handle* handle);
	virtual AUD_Status getStatus(AUD_Handle* handle);
	virtual void lock();
	virtual void unlock();
	virtual bool checkCapability(int capability);
	virtual bool setCapability(int capability, void *value);
	virtual bool getCapability(int capability, void *value);

	virtual AUD_Handle* play3D(AUD_IFactory* factory, bool keep = false);
	virtual bool updateListener(AUD_3DData &data);
	virtual bool setSetting(AUD_3DSetting setting, float value);
	virtual float getSetting(AUD_3DSetting setting);
	virtual bool updateSource(AUD_Handle* handle, AUD_3DData &data);
	virtual bool setSourceSetting(AUD_Handle* handle,
								  AUD_3DSourceSetting setting, float value);
	virtual float getSourceSetting(AUD_Handle* handle,
								   AUD_3DSourceSetting setting);
};

#endif //AUD_OPENALDEVICE
