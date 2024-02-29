/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#ifdef OPENAL_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file OpenALDevice.h
 * @ingroup plugin
 * The OpenALDevice class.
 */

#include "devices/IDevice.h"
#include "devices/IHandle.h"
#include "devices/I3DDevice.h"
#include "devices/I3DHandle.h"
#include "devices/DefaultSynchronizer.h"
#include "util/Buffer.h"

#include <al.h>
#include <alc.h>
#include <list>
#include <mutex>
#include <thread>
#include <string>

AUD_NAMESPACE_BEGIN

/**
 * This device plays through OpenAL.
 */
class AUD_PLUGIN_API OpenALDevice : public IDevice, public I3DDevice
{
private:
	/// Saves the data for playback.
	class OpenALHandle : public IHandle, public I3DHandle
	{
	private:
		friend class OpenALDevice;

		static const int CYCLE_BUFFERS = 3;

		/// Whether it's a buffered or a streamed source.
		bool m_isBuffered;

		/// The reader source.
		std::shared_ptr<IReader> m_reader;

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
		Quaternion m_orientation;

		/// Current status of the handle
		Status m_status;

		/// Whether the source is relative or not.
		ALint m_relative;

		/// Own device.
		OpenALDevice* m_device;

		AUD_LOCAL bool pause(bool keep);

		AUD_LOCAL bool reinitialize();

		// delete copy constructor and operator=
		OpenALHandle(const OpenALHandle&) = delete;
		OpenALHandle& operator=(const OpenALHandle&) = delete;

	public:

		/**
		 * Creates a new OpenAL handle.
		 * \param device The OpenAL device the handle belongs to.
		 * \param format The AL format.
		 * \param reader The reader this handle plays.
		 * \param keep Whether to keep the handle alive when the reader ends.
		 */
		OpenALHandle(OpenALDevice* device, ALenum format, std::shared_ptr<IReader> reader, bool keep);

		virtual ~OpenALHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(double position);
		virtual double getPosition();
		virtual Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);

		virtual Vector3 getLocation();
		virtual bool setLocation(const Vector3& location);
		virtual Vector3 getVelocity();
		virtual bool setVelocity(const Vector3& velocity);
		virtual Quaternion getOrientation();
		virtual bool setOrientation(const Quaternion& orientation);
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
	DeviceSpecs m_specs;

	/**
	 * The device name.
	 */
	std::string m_name;

	/**
	 * Whether the device has the AL_EXT_MCFORMATS extension.
	 */
	bool m_useMC;

	/**
	 * Whether the ALC_EXT_disconnect extension is present and device disconnect should be checked repeatedly.
	 */
	bool m_checkDisconnect;

	/**
	 * The list of sounds that are currently playing.
	 */
	std::list<std::shared_ptr<OpenALHandle> > m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<std::shared_ptr<OpenALHandle> > m_pausedSounds;

	/**
	 * The mutex for locking.
	 */
	std::recursive_mutex m_mutex;

	/**
	 * The streaming thread.
	 */
	std::thread m_thread;

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
	Buffer m_buffer;

	/**
	 * Orientation.
	 */
	Quaternion m_orientation;

	/// Synchronizer.
	DefaultSynchronizer m_synchronizer;

	/**
	 * Starts the streaming thread.
	 * \param Whether the previous thread should be joined.
	 */
	AUD_LOCAL void start();

	/**
	 * Streaming thread main function.
	 */
	AUD_LOCAL void updateStreams();

	/**
	 * Gets the format according to the specs.
	 * \param format The variable to put the format into.
	 * \param specs The specs to read the channel count from.
	 * \return Whether the format is valid or not.
	 */
	AUD_LOCAL bool getFormat(ALenum &format, Specs specs);

	// delete copy constructor and operator=
	OpenALDevice(const OpenALDevice&) = delete;
	OpenALDevice& operator=(const OpenALDevice&) = delete;

public:
	/**
	 * Opens the OpenAL audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \param name The name of the device to be opened.
	 * \note The specification really used for opening the device may differ.
	 * \note The buffersize will be multiplicated by three for this device.
	 * \exception DeviceException Thrown if the audio device cannot be opened.
	 */
	OpenALDevice(DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE, const std::string &name = "");

	virtual ~OpenALDevice();

	virtual DeviceSpecs getSpecs() const;
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<IReader> reader, bool keep = false);
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
	virtual ISynchronizer* getSynchronizer();

	virtual Vector3 getListenerLocation() const;
	virtual void setListenerLocation(const Vector3& location);
	virtual Vector3 getListenerVelocity() const;
	virtual void setListenerVelocity(const Vector3& velocity);
	virtual Quaternion getListenerOrientation() const;
	virtual void setListenerOrientation(const Quaternion& orientation);
	virtual float getSpeedOfSound() const;
	virtual void setSpeedOfSound(float speed);
	virtual float getDopplerFactor() const;
	virtual void setDopplerFactor(float factor);
	virtual DistanceModel getDistanceModel() const;
	virtual void setDistanceModel(DistanceModel model);

	/**
	 * Retrieves a list of available hardware devices to open with OpenAL.
	 * @return The list of devices to open.
	 */
	static std::list<std::string> getDeviceNames();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
