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

void SDLDevice::SDL_mix(void* data, Uint8* buffer, int length)
{
	SDLDevice* device = (SDLDevice*)data;

	if(!device->m_playback)
	{
		SDL_PauseAudio(1);

		std::memset(buffer, 0, length);

		return;
	}

	device->mix((data_t*)buffer, length / AUD_DEVICE_SAMPLE_SIZE(device->m_specs));
}

void SDLDevice::playing(bool playing)
{
	if(!m_playback)
		SDL_PauseAudio(playing ? 0 : 1);

	m_playback = playing;
}

SDLDevice::SDLDevice(DeviceSpecs specs, int buffersize) :
	m_playback(false)
{
	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	m_specs = specs;

	SDL_AudioSpec format, obtained;

	format.freq = m_specs.rate;
	switch(m_specs.format)
	{
	case FORMAT_U8:
		format.format = AUDIO_U8;
		break;
	case FORMAT_S16:
		format.format = AUDIO_S16SYS;
		break;
	case FORMAT_S32:
		format.format = AUDIO_S32SYS;
		break;
	case FORMAT_FLOAT32:
		format.format = AUDIO_F32SYS;
		break;
	default:
		format.format = AUDIO_F32SYS;
		break;
	}

	format.channels = m_specs.channels;
	format.samples = buffersize;
	format.callback = SDLDevice::SDL_mix;
	format.userdata = this;

	if(SDL_OpenAudio(&format, &obtained) != 0)
		AUD_THROW(DeviceException, "The audio device couldn't be opened with SDL.");

	m_specs.rate = (SampleRate)obtained.freq;
	m_specs.channels = (Channels)obtained.channels;

	switch(obtained.format)
	{
	case AUDIO_U8:
		m_specs.format = FORMAT_U8;
		break;
	case AUDIO_S16SYS:
		m_specs.format = FORMAT_S16;
		break;
	case AUDIO_S32SYS:
		m_specs.format = FORMAT_S32;
		break;
	case AUDIO_F32SYS:
		m_specs.format = FORMAT_FLOAT32;
		break;
	default:
		SDL_CloseAudio();
		AUD_THROW(DeviceException, "The sample format obtained from SDL is not supported.");
		break;
	}

	create();
}

SDLDevice::~SDLDevice()
{
	SDL_PauseAudio(1);
	SDL_CloseAudio();

	destroy();
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

	virtual void setName(std::string name)
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
