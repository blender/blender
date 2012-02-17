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

/** \file audaspace/OpenAL/AUD_OpenALDevice.h
 *  \ingroup audopenal
 */


#ifndef __AUD_OPENALDEVICE_H__
#define __AUD_OPENALDEVICE_H__

#include "AUD_IDevice.h"
#include "AUD_IHandle.h"
#include "AUD_I3DDevice.h"
#include "AUD_I3DHandle.h"
#include "AUD_Buffer.h"
//struct AUD_OpenALBufferedFactory;

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
	/// Saves the data for playback.
	class AUD_OpenALHandle : public AUD_IHandle, public AUD_I3DHandle
	{
	public:
		static const int CYCLE_BUFFERS = 3;

		/// Whether it's a buffered or a streamed source.
		bool m_isBuffered;

		/// The reader source.
		AUD_Reference<AUD_IReader> m_reader;

		/// Whether to keep the source if end of it is reached.
		bool m_keep;

		/// OpenAL sample format.
		ALenum m_format;

		/// OpenAL source.
		ALuint m_source;

		/// OpenAL buffers.
		ALuint m_buffers[CYCLE_BUFFERS];

		/// The first buffer to be read next.
		int m_current;

		/// Whether the stream doesn't return any more data.
		bool m_eos;

		/// The loop count of the source.
		int m_loopcount;

		/// The stop callback.
		stopCallback m_stop;

		/// Stop callback data.
		void* m_stop_data;

		/// Orientation.
		AUD_Quaternion m_orientation;

		/// Current status of the handle
		AUD_Status m_status;

		/// Own device.
		AUD_OpenALDevice* m_device;

	public:

		/**
		 * Creates a new OpenAL handle.
		 * \param device The OpenAL device the handle belongs to.
		 * \param format The AL format.
		 * \param reader The reader this handle plays.
		 * \param keep Whether to keep the handle alive when the reader ends.
		 */
		AUD_OpenALHandle(AUD_OpenALDevice* device, ALenum format, AUD_Reference<AUD_IReader> reader, bool keep);

		virtual ~AUD_OpenALHandle() {}
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

		virtual AUD_Vector3 getSourceLocation();
		virtual bool setSourceLocation(const AUD_Vector3& location);
		virtual AUD_Vector3 getSourceVelocity();
		virtual bool setSourceVelocity(const AUD_Vector3& velocity);
		virtual AUD_Quaternion getSourceOrientation();
		virtual bool setSourceOrientation(const AUD_Quaternion& orientation);
		virtual bool isRelative();
		virtual bool setRelative(bool relative);
		virtual float getVolumeMaximum();
		virtual bool setVolumeMaximum(float volume);
		virtual float getVolumeMinimum();
		virtual bool setVolumeMinimum(float volume);
		virtual float getDistanceMaximum();
		virtual bool setDistanceMaximum(float distance);
		virtual float getDistanceReference();
		virtual bool setDistanceReference(float distance);
		virtual float getAttenuation();
		virtual bool setAttenuation(float factor);
		virtual float getConeAngleOuter();
		virtual bool setConeAngleOuter(float angle);
		virtual float getConeAngleInner();
		virtual bool setConeAngleInner(float angle);
		virtual float getConeVolumeOuter();
		virtual bool setConeVolumeOuter(float volume);
	};

	typedef std::list<AUD_Reference<AUD_OpenALHandle> >::iterator AUD_HandleIterator;

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
	std::list<AUD_Reference<AUD_OpenALHandle> > m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<AUD_Reference<AUD_OpenALHandle> > m_pausedSounds;

	/**
	 * The list of buffered factories.
	 */
	//std::list<AUD_OpenALBufferedFactory*>* m_bufferedFactories;

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
	 * Device buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * Orientation.
	 */
	AUD_Quaternion m_orientation;

	/**
	 * Starts the streaming thread.
	 * \param Whether the previous thread should be joined.
	 */
	void start(bool join = true);

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
	virtual AUD_Reference<AUD_IHandle> play(AUD_Reference<AUD_IReader> reader, bool keep = false);
	virtual AUD_Reference<AUD_IHandle> play(AUD_Reference<AUD_IFactory> factory, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);

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
};

#endif //__AUD_OPENALDEVICE_H__
