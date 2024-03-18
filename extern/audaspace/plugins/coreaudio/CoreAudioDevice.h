/*******************************************************************************
 * Copyright 2009-2021 Jörg Müller
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

#ifdef COREAUDIO_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file CoreAudioDevice.h
 * @ingroup plugin
 * The CoreAudioDevice class.
 */

#include "CoreAudioSynchronizer.h"
#include "devices/SoftwareDevice.h"

#include <memory>

#include <AudioUnit/AudioUnit.h>

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through CoreAudio, the Apple audio API.
 */
class AUD_PLUGIN_API CoreAudioDevice : public SoftwareDevice
{
private:
	/**
	 * Whether there is currently playback.
	 */
	bool m_playback;

	/**
	 * The CoreAudio AudioUnit.
	 */
	AudioUnit m_audio_unit;

	/**
	 * The Synchronizer.
	 */
	std::unique_ptr<CoreAudioSynchronizer> m_synchronizer;

	/**
	 * Mixes the next bytes into the buffer.
	 * \param data The CoreAudio device.
	 * \param flags Unused flags.
	 * \param time_stamp Unused time stamp.
	 * \param bus_number Unused bus number.
	 * \param number_frames Unused number of frames.
	 * \param buffer_list The list of buffers to be filled.
	 */
	AUD_LOCAL static OSStatus CoreAudio_mix(void* data, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time_stamp, UInt32 bus_number, UInt32 number_frames, AudioBufferList* buffer_list);

	// delete copy constructor and operator=
	CoreAudioDevice(const CoreAudioDevice&) = delete;
	CoreAudioDevice& operator=(const CoreAudioDevice&) = delete;

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Opens the CoreAudio audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	CoreAudioDevice(DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the CoreAudio audio device.
	 */
	virtual ~CoreAudioDevice();

	virtual ISynchronizer* getSynchronizer();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
