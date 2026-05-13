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

#include "SDLDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "Exception.h"
#include "IReader.h"

AUD_NAMESPACE_BEGIN

void SDLDevice::SDL_mix(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/)
{
	SDLDevice* device = (SDLDevice*)userdata;

	if(!device->m_playback)
		return;

	const int sample_size = AUD_DEVICE_SAMPLE_SIZE(device->m_specs);
	const int num_samples = additional_amount / sample_size;
	data_t* buffer = (data_t*)SDL_stack_alloc(Uint8, additional_amount);
	device->mix(buffer, num_samples);
	SDL_PutAudioStreamData(stream, buffer, additional_amount);
	SDL_stack_free(buffer);
}

void SDLDevice::playing(bool playing)
{
	if(playing)
		SDL_ResumeAudioStreamDevice(m_stream);
	else
		SDL_PauseAudioStreamDevice(m_stream);

	m_playback = playing;
}

SDL_AudioSpec SDLDevice::sdl_audiospec_from_device_specs(const DeviceSpecs &specs)
{
	SDL_AudioSpec audiospec;

	switch(specs.format)
	{
	case FORMAT_U8:
		audiospec.format = SDL_AUDIO_U8;
		break;
	case FORMAT_S16:
		audiospec.format = SDL_AUDIO_S16;
		break;
	case FORMAT_S32:
		audiospec.format = SDL_AUDIO_S32;
		break;
	case FORMAT_FLOAT32:
		audiospec.format = SDL_AUDIO_F32;
		break;
	default:
		audiospec.format = SDL_AUDIO_F32;
		break;
	}

	audiospec.channels = specs.channels;
	audiospec.freq = specs.rate;

	return audiospec;
}

SDLDevice::SDLDevice(DeviceSpecs specs, int buffersize) :
	m_playback(false),
	m_stream(nullptr)
{
	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == static_cast<SampleRate>(RATE_INVALID))
		specs.rate = RATE_48000;

	m_specs = specs;

	if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
		AUD_THROW(DeviceException, "Failed to initialize SDL Audio subsystem.");

	const SDL_AudioSpec audiospec = sdl_audiospec_from_device_specs(specs);
	m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audiospec, SDLDevice::SDL_mix, this);

	if(!m_stream)
		AUD_THROW(DeviceException, "The audio device couldn't be opened with SDL.");

	create();
}

SDLDevice::~SDLDevice()
{
	destroy();

	SDL_DestroyAudioStream(m_stream);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

class SDLDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;

public:
	SDLDeviceFactory() :
		m_buffersize(AUD_DEFAULT_BUFFER_SIZE)
	{
		m_specs.format = FORMAT_S16;
		m_specs.channels = CHANNELS_STEREO;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new SDLDevice(m_specs, m_buffersize));
	}

	virtual int getPriority()
	{
		return 1 << 5;
	}

	virtual void setSpecs(DeviceSpecs specs)
	{
		m_specs = specs;
	}

	virtual void setBufferSize(int buffersize)
	{
		m_buffersize = buffersize;
	}

	virtual void setName(const std::string &name)
	{
	}
};

void SDLDevice::registerPlugin()
{
	DeviceManager::registerDevice("SDL", std::shared_ptr<IDeviceFactory>(new SDLDeviceFactory));
}

#ifdef SDL_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	SDLDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "SDL";
}
#endif

AUD_NAMESPACE_END
