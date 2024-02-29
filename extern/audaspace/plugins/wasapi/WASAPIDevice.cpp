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

HRESULT WASAPIDevice::setupRenderClient(IAudioRenderClient*& render_client, UINT32& buffer_size)
{
	const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

	UINT32 padding;
	UINT32 length;
	data_t* buffer;

	HRESULT result;

	if(FAILED(result = m_audio_client->GetBufferSize(&buffer_size)))
		return result;

	if(FAILED(result = m_audio_client->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&render_client))))
		return result;

	if(FAILED(result = m_audio_client->GetCurrentPadding(&padding)))
		return result;

	length = buffer_size - padding;

	if(FAILED(result = render_client->GetBuffer(length, &buffer)))
		return result;

	mix((data_t*)buffer, length);

	if(FAILED(result = render_client->ReleaseBuffer(length, 0)))
		return result;

	m_audio_client->Start();

	return result;
}

void WASAPIDevice::runMixingThread()
{
	UINT32 buffer_size;

	IAudioRenderClient* render_client = nullptr;

	std::chrono::milliseconds sleep_duration(0);

	bool run_init = true;

	for(;;)
	{
		HRESULT result = S_OK;

		{
			UINT32 padding;
			UINT32 length;
			data_t* buffer;
			std::lock_guard<ILockable> lock(*this);

			if(run_init)
			{
				result = setupRenderClient(render_client, buffer_size);

				if(FAILED(result))
					goto stop_thread;

				sleep_duration = std::chrono::milliseconds(buffer_size * 1000 / int(m_specs.rate) / 2);
			}

			if(m_default_device_changed)
			{
				m_default_device_changed = false;
				result = AUDCLNT_E_DEVICE_INVALIDATED;
				goto stop_thread;
			}

			if(FAILED(result = m_audio_client->GetCurrentPadding(&padding)))
				goto stop_thread;

			length = buffer_size - padding;

			if(FAILED(result = render_client->GetBuffer(length, &buffer)))
				goto stop_thread;

			mix((data_t*)buffer, length);

			if(FAILED(result = render_client->ReleaseBuffer(length, 0)))
				goto stop_thread;

			// stop thread
			if(shouldStop())
			{
				stop_thread:
					m_audio_client->Stop();
					SafeRelease(&render_client);

					if(result == AUDCLNT_E_DEVICE_INVALIDATED)
					{
						DeviceSpecs specs = m_specs;
						if(!setupDevice(specs))
							result = S_FALSE;
						else
						{
							setSpecs(specs);

							run_init = true;
						}
					}

					if(result != AUDCLNT_E_DEVICE_INVALIDATED)
					{
						doStop();
						return;
					}
			}
		}

		std::this_thread::sleep_for(sleep_duration);
	}
}

bool WASAPIDevice::setupDevice(DeviceSpecs &specs)
{
	SafeRelease(&m_audio_client);
	SafeRelease(&m_imm_device);

	const IID IID_IAudioClient = __uuidof(IAudioClient);

	if(FAILED(m_imm_device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_imm_device)))
		return false;

	if(FAILED(m_imm_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_audio_client))))
		return false;

	WAVEFORMATEXTENSIBLE wave_format_extensible_closest_match;
	WAVEFORMATEXTENSIBLE* closest_match_pointer = &wave_format_extensible_closest_match;

	REFERENCE_TIME minimum_time = 0;
	REFERENCE_TIME buffer_duration;

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

	HRESULT result = m_audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, reinterpret_cast<const WAVEFORMATEX*>(&m_wave_format_extensible), reinterpret_cast<WAVEFORMATEX**>(&closest_match_pointer));

	if(result == S_FALSE)
	{
		bool errored = false;

		if(closest_match_pointer->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
			goto closest_match_error;

		specs.channels = Channels(closest_match_pointer->Format.nChannels);
		specs.rate = closest_match_pointer->Format.nSamplesPerSec;

		if(closest_match_pointer->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
		{
			if(closest_match_pointer->Format.wBitsPerSample == 32)
				specs.format = FORMAT_FLOAT32;
			else if(closest_match_pointer->Format.wBitsPerSample == 64)
				specs.format = FORMAT_FLOAT64;
			else
				goto closest_match_error;
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
				goto closest_match_error;
				break;
			}
		}
		else
			goto closest_match_error;

		m_wave_format_extensible = *closest_match_pointer;

		if(false)
		{
			closest_match_error:
			errored = true;
		}

		if(closest_match_pointer != &wave_format_extensible_closest_match)
		{
			CoTaskMemFree(closest_match_pointer);
			closest_match_pointer = &wave_format_extensible_closest_match;
		}

		if(errored)
			return false;
	}
	else if(FAILED(result))
		return false;

	if(FAILED(m_audio_client->GetDevicePeriod(nullptr, &minimum_time)))
		return false;

	buffer_duration = REFERENCE_TIME(m_buffersize) * REFERENCE_TIME(10000000) / REFERENCE_TIME(specs.rate);

	if(minimum_time > buffer_duration)
		buffer_duration = minimum_time;

	if(FAILED(m_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, reinterpret_cast<WAVEFORMATEX*>(&m_wave_format_extensible), nullptr)))
		return false;

	return true;
}

ULONG WASAPIDevice::AddRef()
{
	return InterlockedIncrement(&m_reference_count);
}

ULONG WASAPIDevice::Release()
{
	ULONG reference_count = InterlockedDecrement(&m_reference_count);

	if(0 == reference_count)
		delete this;

	return reference_count;
}

HRESULT WASAPIDevice::QueryInterface(REFIID riid, void **ppvObject)
{
	if(riid == __uuidof(IMMNotificationClient))
	{
		*ppvObject = reinterpret_cast<IMMNotificationClient*>(this);
		AddRef();
	}
	else if(riid == IID_IUnknown)
	{
		*ppvObject = reinterpret_cast<IUnknown*>(this);
		AddRef();
	}
	else
	{
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	return S_OK;
}

HRESULT WASAPIDevice::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	return S_OK;
}

HRESULT WASAPIDevice::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT WASAPIDevice::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

HRESULT WASAPIDevice::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
	if(flow != EDataFlow::eCapture)
		m_default_device_changed = true;

	return S_OK;
}

HRESULT WASAPIDevice::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
	return S_OK;
}

WASAPIDevice::WASAPIDevice(DeviceSpecs specs, int buffersize) :
	m_buffersize(buffersize),
	m_imm_device_enumerator(nullptr),
	m_imm_device(nullptr),
	m_audio_client(nullptr),
	m_wave_format_extensible({}),
	m_default_device_changed(false),
	m_reference_count(1)
{
	// initialize COM if it hasn't happened yet
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	if(FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&m_imm_device_enumerator))))
		goto error;

	if(!setupDevice(specs))
		goto error;

	m_specs = specs;

	create();

	m_imm_device_enumerator->RegisterEndpointNotificationCallback(this);

	return;

	error:
	SafeRelease(&m_imm_device);
	SafeRelease(&m_imm_device_enumerator);
	SafeRelease(&m_audio_client);
	AUD_THROW(DeviceException, "The audio device couldn't be opened with WASAPI.");
}

WASAPIDevice::~WASAPIDevice()
{
	stopMixingThread();

	m_imm_device_enumerator->UnregisterEndpointNotificationCallback(this);

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

	virtual void setName(const std::string &name)
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
