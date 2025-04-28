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

#include <pipewire/pipewire.h>

#include "devices/MixingThreadDevice.h"

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through PipeWire, the simple direct media layer.
 */
class AUD_PLUGIN_API PipeWireDevice : public MixingThreadDevice
{
private:
	pw_stream* m_stream;
	pw_thread_loop* m_thread;
	std::unique_ptr<pw_stream_events> m_events;
	bool m_active{false};

	/// Synchronizer.
	bool m_getSynchronizerStartTime{false};
	int64_t m_synchronizerStartTime{0};
	double m_synchronizerStartPosition{0.0};

	AUD_LOCAL static void handleStateChanged(void* device_ptr, enum pw_stream_state old, enum pw_stream_state state, const char* error);

	/**
	 * Mixes the next bytes into the buffer.
	 * \param data The PipeWire device.
	 */
	AUD_LOCAL static void mixAudioBuffer(void* device_ptr);

	// delete copy constructor and operator=
	PipeWireDevice(const PipeWireDevice&) = delete;
	PipeWireDevice& operator=(const PipeWireDevice&) = delete;

protected:
	void preMixingWork(bool playing);
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

	virtual void seekSynchronizer(double time);
	virtual double getSynchronizerPosition();
	virtual void playSynchronizer();
	virtual void stopSynchronizer();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
