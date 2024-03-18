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

#ifdef JACK_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file JackDevice.h
 * @ingroup plugin
 * The JackDevice class.
 */

#include "JackSynchronizer.h"
#include "devices/SoftwareDevice.h"
#include "util/Buffer.h"

#include <string>
#include <condition_variable>
#include <thread>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through JACK.
 */
class AUD_PLUGIN_API JackDevice : public SoftwareDevice
{
private:
	/**
	 * The output ports of jack.
	 */
	jack_port_t** m_ports;

	/**
	 * The jack client.
	 */
	jack_client_t* m_client;

	/**
	 * The output buffer.
	 */
	Buffer m_buffer;

	/**
	 * The deinterleaving buffer.
	 */
	Buffer m_deinterleavebuf;

	jack_ringbuffer_t** m_ringbuffers;

	/**
	 * Whether the device is valid.
	 */
	bool m_valid;

	/// Synchronizer.
	JackSynchronizer m_synchronizer;

	/**
	 * Invalidates the jack device.
	 * \param data The jack device that gets invalidet by jack.
	 */
	AUD_LOCAL static void jack_shutdown(void* data);

	/**
	 * Mixes the next bytes into the buffer.
	 * \param length The length in samples to be filled.
	 * \param data A pointer to the jack device.
	 * \return 0 what shows success.
	 */
	AUD_LOCAL static int jack_mix(jack_nframes_t length, void* data);

	AUD_LOCAL static int jack_sync(jack_transport_state_t state, jack_position_t* pos, void* data);

	/**
	 * Next JACK Transport state (-1 if not expected to change).
	 */
	jack_transport_state_t m_nextState;

	/**
	 * Current jack transport status.
	 */
	jack_transport_state_t m_state;

	/**
	 * Syncronisation state.
	 */
	int m_sync;

	/**
	 * External syncronisation callback function.
	 */
	ISynchronizer::syncFunction m_syncFunc;

	/**
	 * Data for the sync function.
	 */
	void* m_syncFuncData;

	/**
	 * The mixing thread.
	 */
	std::thread m_mixingThread;

	/**
	 * Mutex for mixing.
	 */
	std::mutex m_mixingLock;

	/**
	 * Condition for mixing.
	 */
	std::condition_variable m_mixingCondition;

	/**
	 * Updates the ring buffers.
	 */
	AUD_LOCAL void updateRingBuffers();

	// delete copy constructor and operator=
	JackDevice(const JackDevice&) = delete;
	JackDevice& operator=(const JackDevice&) = delete;

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Creates a JACK client for audio output.
	 * \param name The client name.
	 * \param specs The wanted audio specification, where only the channel count
	 *              is important.
	 * \param buffersize The size of the internal buffer.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	JackDevice(std::string name, DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the JACK client.
	 */
	virtual ~JackDevice();

	virtual ISynchronizer* getSynchronizer();

	/**
	 * Starts jack transport playback.
	 */
	void startPlayback();

	/**
	 * Stops jack transport playback.
	 */
	void stopPlayback();

	/**
	 * Seeks jack transport playback.
	 * \param time The time to seek to.
	 */
	void seekPlayback(double time);

	/**
	 * Sets the sync callback for jack transport playback.
	 * \param sync The callback function.
	 * \param data The data for the function.
	 */
	void setSyncCallback(ISynchronizer::syncFunction sync, void* data);

	/**
	 * Retrieves the jack transport playback time.
	 * \return The current time position.
	 */
	double getPlaybackPosition();

	/**
	 * Returns whether jack transport plays back.
	 * \return Whether jack transport plays back.
	 */
	bool doesPlayback();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
