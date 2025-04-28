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

#include <atomic>
#include <condition_variable>
#include <string>
#include <thread>

#include <jack/jack.h>

#include "devices/MixingThreadDevice.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through JACK.
 */
class AUD_PLUGIN_API JackDevice : public MixingThreadDevice
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
	 * The deinterleaving buffer.
	 */
	Buffer m_deinterleavebuf;

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
	 * Last known JACK Transport state used for stop callbacks.
	 */
	jack_transport_state_t m_lastState;

	/**
	 * Last known JACK Transport state used for stop callbacks.
	 */
	jack_transport_state_t m_lastMixState;

	/**
	 * Time for a synchronisation request.
	 */
	std::atomic<float> m_syncTime;

	/**
	 * Sync revision used to notify the mixing thread that a sync call is necessary.
	 */
	std::atomic<int> m_syncCallRevision;

	/**
	 * The sync revision that the last sync call in the mixing thread handled.
	 */
	std::atomic<int> m_lastSyncCallRevision;

	/**
	 * Sync revision that is increased every time jack transport enters the rolling state.
	 */
	int m_rollingSyncRevision;

	/**
	 * The last time the jack_sync callback saw the rolling sync revision.
	 *
	 * Used to ensure the sync callback will be called when consecutive syncs target the same sync time.
	 */
	int m_lastRollingSyncRevision;

	/**
	 * External syncronisation callback function.
	 */
	syncFunction m_syncFunc;

	/**
	 * Data for the sync function.
	 */
	void* m_syncFuncData;

	AUD_LOCAL void preMixingWork(bool playing) override;

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
	JackDevice(const std::string &name, DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the JACK client.
	 */
	virtual ~JackDevice();

	/**
	 * Starts jack transport playback.
	 */
	void playSynchronizer();

	/**
	 * Stops jack transport playback.
	 */
	void stopSynchronizer();

	/**
	 * Seeks jack transport playback.
	 * \param time The time to seek to.
	 */
	void seekSynchronizer(double time);

	/**
	 * Sets the sync callback for jack transport playback.
	 * \param sync The callback function.
	 * \param data The data for the function.
	 */
	void setSyncCallback(syncFunction sync, void* data);

	/**
	 * Retrieves the jack transport playback time.
	 * \return The current time position.
	 */
	double getSynchronizerPosition();

	/**
	 * Returns whether jack transport plays back.
	 * \return Whether jack transport plays back.
	 */
	int isSynchronizerPlaying();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
