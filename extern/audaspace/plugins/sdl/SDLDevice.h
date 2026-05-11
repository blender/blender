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

#ifdef SDL_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file SDLDevice.h
 * @ingroup plugin
 * The SDLDevice class.
 */

#include "devices/SoftwareDevice.h"

#include <SDL3/SDL.h>

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through SDL, the simple direct media layer.
 */
class AUD_PLUGIN_API SDLDevice : public SoftwareDevice
{
private:
	/**
	 * Whether there is currently playback.
	 */
	bool m_playback;

	/**
	 * The SDL audio stream.
	 */
	SDL_AudioStream* m_stream;

	/**
	 * SDL callback to mix the next bytes into the audio stream.
	 * Uses the SDL_AudioStreamCallback signature.
	 * \param userdata The SDL device.
	 * \param stream The target audio stream.
	 * \param additional_amount The number of bytes needed.
	 * \param total_amount The total amount of data buffered.
	 */
	AUD_LOCAL static void SDL_mix(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

	/**
	 * Helper function to convert Audaspace DeviceSpecs structs to SDL SDL_AudioSpec structs.
	 */
	AUD_LOCAL static SDL_AudioSpec sdl_audiospec_from_device_specs(const DeviceSpecs &specs);

	// delete copy constructor and operator=
	SDLDevice(const SDLDevice&) = delete;
	SDLDevice& operator=(const SDLDevice&) = delete;

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Opens the SDL audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	SDLDevice(DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the SDL audio device.
	 */
	virtual ~SDLDevice();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
