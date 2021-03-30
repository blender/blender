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

#ifdef PULSEAUDIO_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file PulseAudioDevice.h
 * @ingroup plugin
 * The PulseAudioDevice class.
 */

#include "devices/ThreadedDevice.h"

#include <pulse/pulseaudio.h>

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through PulseAudio, the simple direct media layer.
 */
class AUD_PLUGIN_API PulseAudioDevice : public ThreadedDevice
{
private:
	pa_mainloop* m_mainloop;
	pa_context* m_context;
	pa_stream* m_stream;
	pa_context_state_t m_state;

	int m_buffersize;
	uint32_t m_underflows;

	/**
	 * Reports the state of the PulseAudio server connection.
	 * \param context The PulseAudio context.
	 * \param data The PulseAudio device.
	 */
	AUD_LOCAL static void PulseAudio_state_callback(pa_context* context, void* data);

	/**
	 * Supplies the next samples to PulseAudio.
	 * \param stream The PulseAudio stream.
	 * \param num_bytes The length in bytes to be supplied.
	 * \param data The PulseAudio device.
	 */
	AUD_LOCAL static void PulseAudio_request(pa_stream* stream, size_t num_bytes, void* data);

	/**
	 * Reports an underflow from the PulseAudio server.
	 * Automatically adjusts the latency if this happens too often.
	 * @param stream The PulseAudio stream.
	 * \param data The PulseAudio device.
	 */
	AUD_LOCAL static void PulseAudio_underflow(pa_stream* stream, void* data);

	/**
	 * Streaming thread main function.
	 */
	AUD_LOCAL void runMixingThread();

	// delete copy constructor and operator=
	PulseAudioDevice(const PulseAudioDevice&) = delete;
	PulseAudioDevice& operator=(const PulseAudioDevice&) = delete;

public:
	/**
	 * Opens the PulseAudio audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	PulseAudioDevice(std::string name, DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the PulseAudio audio device.
	 */
	virtual ~PulseAudioDevice();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
