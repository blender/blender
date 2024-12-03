/*******************************************************************************
 * Copyright 2009-2024 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#ifdef PIPEWIRE_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file PipeWireDevice.h
 * @ingroup plugin
 * The PipeWireDevice class.
 */

#include <condition_variable>
#include <thread>
#include <pipewire/pipewire.h>
#include <spa/utils/ringbuffer.h>

#include "devices/SoftwareDevice.h"

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through PipeWire, the simple direct media layer.
 */
class AUD_PLUGIN_API PipeWireDevice : public SoftwareDevice
{
private:
	class PipeWireSynchronizer : public DefaultSynchronizer
	{
		PipeWireDevice* m_device;
		bool m_playing = false;
		bool m_get_tick_start = false;
		int64_t m_tick_start = 0.0f;
		double m_seek_pos = 0.0f;

	public:
		PipeWireSynchronizer(PipeWireDevice* device);

		void updateTickStart();
		virtual void play();
		virtual void stop();
		virtual void seek(std::shared_ptr<IHandle> handle, double time);
		virtual double getPosition(std::shared_ptr<IHandle> handle);
	};

	/// Synchronizer.
	PipeWireSynchronizer m_synchronizer;

	/**
	 * Whether we should start filling our ringbuffer with audio.
	 */
	bool m_fill_ringbuffer;

	pw_stream* m_stream;
	pw_thread_loop* m_thread;
	std::unique_ptr<pw_stream_events> m_events;

	/**
	 * The mixing thread.
	 */
	std::thread m_mixingThread;
	bool m_run_mixing_thread;

	/**
	 * Mutex for mixing.
	 */
	std::mutex m_mixingLock;

	/**
	 * The mixing ringbuffer and mixing data
	 */
	spa_ringbuffer m_ringbuffer;
	Buffer m_ringbuffer_data;
	std::condition_variable m_mixingCondition;

	AUD_LOCAL static void handleStateChanged(void* device_ptr, enum pw_stream_state old, enum pw_stream_state state, const char* error);

	/**
	 * Updates the ring buffers.
	 */
	AUD_LOCAL void updateRingBuffers();

	/**
	 * Mixes the next bytes into the buffer.
	 * \param data The PipeWire device.
	 */
	AUD_LOCAL static void mixAudioBuffer(void* device_ptr);

	// delete copy constructor and operator=
	PipeWireDevice(const PipeWireDevice&) = delete;
	PipeWireDevice& operator=(const PipeWireDevice&) = delete;

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Opens the PipeWire audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	PipeWireDevice(const std::string& name, DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the PipeWire audio device.
	 */
	virtual ~PipeWireDevice();

	virtual ISynchronizer* getSynchronizer();
	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
