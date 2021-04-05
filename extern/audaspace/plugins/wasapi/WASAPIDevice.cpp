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

#include "WASAPIDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "Exception.h"
#include "IReader.h"

AUD_NAMESPACE_BEGIN

template <class T> void SafeRelease(T **ppT)
{
	if(*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

void WASAPIDevice::runMixingThread()
{
	UINT32 buffer_size;
	UINT32 padding;
	UINT32 length;
	data_t* buffer;

	IAudioRenderClient* render_client = nullptr;

	{
		std::lock_guard<ILockable> lock(*this);

		const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

		if(FAILED(m_audio_client->GetBufferSize(&buffer_size)))
			goto init_error;

		if(FAILED(m_audio_client->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&render_client))))
			goto init_error;

		if(FAILED(m_audio_client->GetCurrentPadding(&padding)))
			goto init_error;

		length = buffer_size - padding;

		if(FAILED(render_client->GetBuffer(length, &buffer)))
			goto init_error;

		mix((data_t*)buffer, length);

		if(FAILED(render_client->ReleaseBuffer(length, 0)))
		{
			init_error:
				SafeRelease(&render_client);
				doStop();
				return;
		}
	}

	m_audio_client->Start();

	auto sleepDuration = std::chrono::milliseconds(buffer_size * 1000 / int(m_specs.rate) / 2);

	for(;;)
	{
		{
			std::lock_guard<ILockable> lock(*this);

			if(FAILED(m_audio_client->GetCurrentPadding(&padding)))
				goto stop_thread;

			length = buffer_size - padding;

			if(FAILED(render_client->GetBuffer(length, &buffer)))
				goto stop_thread;

			mix((data_t*)buffer, length);

			if(FAILED(render_client->ReleaseBuffer(length, 0)))
				goto stop_thread;

			// stop thread
			if(shouldStop())
			{
				stop_thread:
					m_audio_client->Stop();
					SafeRelease(&render_client);
					doStop();
					return;
			}
		}

		std::this_thread::sleep_for(sleepDuration);
	}
}

WASAPIDevice::WASAPIDevice(DeviceSpecs specs, int buffersize) :
	m_imm_device_enumerator(nullptr),
	m_imm_device(nullptr),
	m_audio_client(nullptr),

	m_wave_format_extensible({})
{
	// initialize COM if it hasn't happened yet
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);

	WAVEFORMATEXTENSIBLE wave_format_extensible_closest_match;
	WAVEFORMATEXTENSIBLE* closest_match_pointer = &wave_format_extensible_closest_match;

	HRESULT result;

	REFERENCE_TIME minimum_time = 0;
	REFERENCE_TIME buffer_duration;

	if(FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&m_imm_device_enumerator))))
		goto error;

	if(FAILED(m_imm_device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_imm_device)))
		goto error;

	if(FAILED(m_imm_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_audio_client))))
		goto error;

	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	switch(specs.format)
	{
	case FORMAT_U8:
	case FORMAT_S16:
	case FORMAT_S24:
	case FORMAT_S32:
		m_wave_format_extensible.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
		break;
	case FORMAT_FLOAT32:
		m_wave_format_extensible.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
		break;
	default:
		m_wave_format_extensible.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
		specs.format = FORMAT_FLOAT32;
		break;
	}

	switch(specs.channels)
	{
	case CHANNELS_MONO:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_CENTER;
		break;
	case CHANNELS_STEREO:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		break;
	case CHANNELS_STEREO_LFE:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY;
		break;
	case CHANNELS_SURROUND4:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
		break;
	case CHANNELS_SURROUND5:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
		break;
	case CHANNELS_SURROUND51:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
		break;
	case CHANNELS_SURROUND61:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
		break;
	case CHANNELS_SURROUND71:
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
		break;
	default:
		specs.channels = CHANNELS_STEREO;
		m_wave_format_extensible.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		break;
	}

	m_wave_format_extensible.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	m_wave_format_extensible.Format.nChannels = specs.channels;
	m_wave_format_extensible.Format.nSamplesPerSec = specs.rate;
	m_wave_format_extensible.Format.nAvgBytesPerSec = specs.rate * AUD_DEVICE_SAMPLE_SIZE(specs);
	m_wave_format_extensible.Format.nBlockAlign = AUD_DEVICE_SAMPLE_SIZE(specs);
	m_wave_format_extensible.Format.wBitsPerSample = AUD_FORMAT_SIZE(specs.format) * 8;
	m_wave_format_extensible.Format.cbSize = 22;
	m_wave_format_extensible.Samples.wValidBitsPerSample = m_wave_format_extensible.Format.wBitsPerSample;

	result = m_audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, reinterpret_cast<const WAVEFORMATEX*>(&m_wave_format_extensible), reinterpret_cast<WAVEFORMATEX**>(&closest_match_pointer));

	if(result == S_FALSE)
	{
		if(closest_match_pointer->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
			goto error;

		specs.channels = Channels(closest_match_pointer->Format.nChannels);
		specs.rate = closest_match_pointer->Format.nSamplesPerSec;

		if(closest_match_pointer->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
		{
			if(closest_match_pointer->Format.wBitsPerSample == 32)
				specs.format = FORMAT_FLOAT32;
			else if(closest_match_pointer->Format.wBitsPerSample == 64)
				specs.format = FORMAT_FLOAT64;
			else
				goto error;
		}
		else if(closest_match_pointer->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
		{
			switch(closest_match_pointer->Format.wBitsPerSample)
			{
			case 8:
				specs.format = FORMAT_U8;
				break;
			case 16:
				specs.format = FORMAT_S16;
				break;
			case 24:
				specs.format = FORMAT_S24;
				break;
			case 32:
				specs.format = FORMAT_S32;
				break;
			default:
				goto error;
				break;
			}
		}
		else
			goto error;

		m_wave_format_extensible = *closest_match_pointer;

		if(closest_match_pointer != &wave_format_extensible_closest_match)
		{
			CoTaskMemFree(closest_match_pointer);
			closest_match_pointer = &wave_format_extensible_closest_match;
		}
	}
	else if(FAILED(result))
		goto error;

	if(FAILED(m_audio_client->GetDevicePeriod(nullptr, &minimum_time)))
		goto error;

	buffer_duration = REFERENCE_TIME(buffersize) * REFERENCE_TIME(10000000) / REFERENCE_TIME(specs.rate);

	if(minimum_time > buffer_duration)
		buffer_duration = minimum_time;

	m_specs = specs;

	if(FAILED(m_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, reinterpret_cast<WAVEFORMATEX*>(&m_wave_format_extensible), nullptr)))
		goto error;

	create();

	return;

	error:
	if(closest_match_pointer != &wave_format_extensible_closest_match)
		CoTaskMemFree(closest_match_pointer);
	SafeRelease(&m_imm_device);
	SafeRelease(&m_imm_device_enumerator);
	SafeRelease(&m_audio_client);
	AUD_THROW(DeviceException, "The audio device couldn't be opened with WASAPI.");
}

WASAPIDevice::~WASAPIDevice()
{
	stopMixingThread();

	SafeRelease(&m_audio_client);
	SafeRelease(&m_imm_device);
	SafeRelease(&m_imm_device_enumerator);

	destroy();
}

class WASAPIDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;

public:
	WASAPIDeviceFactory() :
		m_buffersize(AUD_DEFAULT_BUFFER_SIZE)
	{
		m_specs.format = FORMAT_S16;
		m_specs.channels = CHANNELS_STEREO;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new WASAPIDevice(m_specs, m_buffersize));
	}

	virtual int getPriority()
	{
		return 1 << 15;
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

void WASAPIDevice::registerPlugin()
{
	DeviceManager::registerDevice("WASAPI", std::shared_ptr<IDeviceFactory>(new WASAPIDeviceFactory));
}

#ifdef WASAPI_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	WASAPIDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "WASAPI";
}
#endif

AUD_NAMESPACE_END
