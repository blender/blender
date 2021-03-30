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

#include "PulseAudioDevice.h"
#include "PulseAudioLibrary.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "Exception.h"
#include "IReader.h"

AUD_NAMESPACE_BEGIN

void PulseAudioDevice::PulseAudio_state_callback(pa_context *context, void *data)
{
	PulseAudioDevice* device = (PulseAudioDevice*)data;

	std::lock_guard<ILockable> lock(*device);

	device->m_state = AUD_pa_context_get_state(context);
}

void PulseAudioDevice::PulseAudio_request(pa_stream *stream, size_t num_bytes, void *data)
{
	PulseAudioDevice* device = (PulseAudioDevice*)data;

	void* buffer;

	AUD_pa_stream_begin_write(stream, &buffer, &num_bytes);

	device->mix((data_t*)buffer, num_bytes / AUD_DEVICE_SAMPLE_SIZE(device->m_specs));

	AUD_pa_stream_write(stream, buffer, num_bytes, nullptr, 0, PA_SEEK_RELATIVE);
}

void PulseAudioDevice::PulseAudio_underflow(pa_stream *stream, void *data)
{
	PulseAudioDevice* device = (PulseAudioDevice*)data;

	DeviceSpecs specs = device->getSpecs();

	if(++device->m_underflows > 4 && device->m_buffersize < AUD_DEVICE_SAMPLE_SIZE(specs) * specs.rate * 2)
	{
		device->m_buffersize <<= 1;
		device->m_underflows = 0;

		pa_buffer_attr buffer_attr;

		buffer_attr.fragsize = -1U;
		buffer_attr.maxlength = -1U;
		buffer_attr.minreq = -1U;
		buffer_attr.prebuf = -1U;
		buffer_attr.tlength = device->m_buffersize;

		AUD_pa_stream_set_buffer_attr(stream, &buffer_attr, nullptr, nullptr);
	}
}

void PulseAudioDevice::runMixingThread()
{
	for(;;)
	{
		{
			std::lock_guard<ILockable> lock(*this);

			if(shouldStop())
			{
				AUD_pa_stream_cork(m_stream, 1, nullptr, nullptr);
				doStop();
				return;
			}
		}

		if(AUD_pa_stream_is_corked(m_stream))
			AUD_pa_stream_cork(m_stream, 0, nullptr, nullptr);

		AUD_pa_mainloop_iterate(m_mainloop, true, nullptr);
	}
}

PulseAudioDevice::PulseAudioDevice(std::string name, DeviceSpecs specs, int buffersize) :
	m_state(PA_CONTEXT_UNCONNECTED),
	m_buffersize(buffersize),
	m_underflows(0)
{
	m_mainloop = AUD_pa_mainloop_new();

	m_context = AUD_pa_context_new(AUD_pa_mainloop_get_api(m_mainloop), name.c_str());

	if(!m_context)
	{
		AUD_pa_mainloop_free(m_mainloop);

		AUD_THROW(DeviceException, "Could not connect to PulseAudio.");
	}

	AUD_pa_context_set_state_callback(m_context, PulseAudio_state_callback, this);

	AUD_pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

	while(m_state != PA_CONTEXT_READY)
	{
		switch(m_state)
		{
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			AUD_pa_context_disconnect(m_context);
			AUD_pa_context_unref(m_context);

			AUD_pa_mainloop_free(m_mainloop);

			AUD_THROW(DeviceException, "Could not connect to PulseAudio.");
			break;
		default:
			AUD_pa_mainloop_iterate(m_mainloop, true, nullptr);
			break;
		}
	}

	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	m_specs = specs;

	pa_sample_spec sample_spec;

	sample_spec.channels = specs.channels;
	sample_spec.format = PA_SAMPLE_FLOAT32;
	sample_spec.rate = specs.rate;

	switch(m_specs.format)
	{
	case FORMAT_U8:
		sample_spec.format = PA_SAMPLE_U8;
		break;
	case FORMAT_S16:
		sample_spec.format = PA_SAMPLE_S16NE;
		break;
	case FORMAT_S24:
		sample_spec.format = PA_SAMPLE_S24NE;
		break;
	case FORMAT_S32:
		sample_spec.format = PA_SAMPLE_S32NE;
		break;
	case FORMAT_FLOAT32:
		sample_spec.format = PA_SAMPLE_FLOAT32;
		break;
	case FORMAT_FLOAT64:
		m_specs.format = FORMAT_FLOAT32;
		break;
	default:
		break;
	}

	m_stream = AUD_pa_stream_new(m_context, "Playback", &sample_spec, nullptr);

	if(!m_stream)
	{
		AUD_pa_context_disconnect(m_context);
		AUD_pa_context_unref(m_context);

		AUD_pa_mainloop_free(m_mainloop);

		AUD_THROW(DeviceException, "Could not create PulseAudio stream.");
	}

	AUD_pa_stream_set_write_callback(m_stream, PulseAudio_request, this);
	AUD_pa_stream_set_underflow_callback(m_stream, PulseAudio_underflow, this);

	pa_buffer_attr buffer_attr;

	buffer_attr.fragsize = -1U;
	buffer_attr.maxlength = -1U;
	buffer_attr.minreq = -1U;
	buffer_attr.prebuf = -1U;
	buffer_attr.tlength = buffersize;

	if(AUD_pa_stream_connect_playback(m_stream, nullptr, &buffer_attr, static_cast<pa_stream_flags_t>(PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE), nullptr, nullptr) < 0)
	{
		AUD_pa_context_disconnect(m_context);
		AUD_pa_context_unref(m_context);

		AUD_pa_mainloop_free(m_mainloop);

		AUD_THROW(DeviceException, "Could not connect PulseAudio stream.");
	}

	create();
}

PulseAudioDevice::~PulseAudioDevice()
{
	stopMixingThread();

	AUD_pa_context_disconnect(m_context);
	AUD_pa_context_unref(m_context);

	AUD_pa_mainloop_free(m_mainloop);

	destroy();
}

class PulseAudioDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;
	std::string m_name;

public:
	PulseAudioDeviceFactory() :
		m_buffersize(AUD_DEFAULT_BUFFER_SIZE),
		m_name("Audaspace")
	{
		m_specs.format = FORMAT_FLOAT32;
		m_specs.channels = CHANNELS_STEREO;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new PulseAudioDevice(m_name, m_specs, m_buffersize));
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
		m_name = name;
	}
};

void PulseAudioDevice::registerPlugin()
{
	if(loadPulseAudio())
		DeviceManager::registerDevice("PulseAudio", std::shared_ptr<IDeviceFactory>(new PulseAudioDeviceFactory));
}

#ifdef PULSEAUDIO_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	PulseAudioDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "PulseAudio";
}
#endif

AUD_NAMESPACE_END
