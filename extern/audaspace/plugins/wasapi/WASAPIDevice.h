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

#ifdef WASAPI_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file WASAPIDevice.h
 * @ingroup plugin
 * The WASAPIDevice class.
 */

#include "devices/ThreadedDevice.h"

#include <thread>

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

AUD_NAMESPACE_BEGIN

/**
 * This device plays back through WASAPI, the Windows audio API.
 */
class AUD_PLUGIN_API WASAPIDevice : IMMNotificationClient, public ThreadedDevice
{
private:
	int m_buffersize;
	IMMDeviceEnumerator* m_imm_device_enumerator;
	IMMDevice* m_imm_device;
	IAudioClient* m_audio_client;
	WAVEFORMATEXTENSIBLE m_wave_format_extensible;
	bool m_default_device_changed;
	LONG m_reference_count;

	AUD_LOCAL HRESULT setupRenderClient(IAudioRenderClient*& render_client, UINT32& buffer_size);

	/**
	 * Streaming thread main function.
	 */
	AUD_LOCAL void runMixingThread();

	AUD_LOCAL bool setupDevice(DeviceSpecs& specs);

	// IUnknown implementation
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
	// IMMNotificationClient implementation
	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);
	HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId);
	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

	// delete copy constructor and operator=
	WASAPIDevice(const WASAPIDevice&) = delete;
	WASAPIDevice& operator=(const WASAPIDevice&) = delete;

public:
	/**
	 * Opens the WASAPI audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	WASAPIDevice(DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the WASAPI audio device.
	 */
	virtual ~WASAPIDevice();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
