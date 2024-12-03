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

#include "PipeWireDevice.h"

#include <spa/param/audio/format-utils.h>

#include "Exception.h"
#include "IReader.h"
#include "PipeWireLibrary.h"

#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"

AUD_NAMESPACE_BEGIN

PipeWireDevice::PipeWireSynchronizer::PipeWireSynchronizer(PipeWireDevice* device) : m_device(device)
{
}

void PipeWireDevice::PipeWireSynchronizer::updateTickStart()
{
	if (!m_get_tick_start)
	{
		return;
	}
	pw_time tm;
	AUD_pw_stream_get_time_n(m_device->m_stream, &tm, sizeof(tm));
	m_tick_start = tm.ticks;
	m_get_tick_start = false;
}

void PipeWireDevice::PipeWireSynchronizer::play()
{
	m_playing = true;
	m_get_tick_start = true;
}

void PipeWireDevice::PipeWireSynchronizer::stop()
{
	std::shared_ptr<IHandle> dummy_handle;
	m_seek_pos = getPosition(dummy_handle);
	m_playing = false;
}

void PipeWireDevice::PipeWireSynchronizer::seek(std::shared_ptr<IHandle> handle, double time)
{
	/* Update start time here as we might update the seek position while playing back. */
	m_get_tick_start = true;
	m_seek_pos = time;
	handle->seek(time);
}

double PipeWireDevice::PipeWireSynchronizer::getPosition(std::shared_ptr<IHandle> handle)
{
	if (!m_playing || m_get_tick_start)
	{
		return m_seek_pos;
	}
	pw_time tm;
	AUD_pw_stream_get_time_n(m_device->m_stream, &tm, sizeof(tm));
	uint64_t now = AUD_pw_stream_get_nsec(m_device->m_stream);
	int64_t diff = now - tm.now;
	/* Elapsed time since the last sample was queued. */
	int64_t elapsed = (tm.rate.denom * diff) / (tm.rate.num * SPA_NSEC_PER_SEC);

	/* Calculate the elapsed time in seconds from the last seek position. */
	double elapsed_time = (tm.ticks - m_tick_start + elapsed) * tm.rate.num / double(tm.rate.denom);
	return elapsed_time + m_seek_pos;
}

void PipeWireDevice::handleStateChanged(void* device_ptr, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
	PipeWireDevice* device = (PipeWireDevice*) device_ptr;
	//fprintf(stderr, "stream state: \"%s\"\n", pw_stream_state_as_string(state));
	if (state == PW_STREAM_STATE_PAUSED)
	{
		AUD_pw_stream_flush(device->m_stream, false);
	}
}

void PipeWireDevice::updateRingBuffers()
{
	uint32_t samplesize = AUD_DEVICE_SAMPLE_SIZE(m_specs);

	sample_t* rb_data = m_ringbuffer_data.getBuffer();
	uint32_t rb_size = m_ringbuffer_data.getSize();
	uint32_t rb_index;
	Buffer mix_buffer = Buffer(rb_size);
	sample_t* mix_buffer_data = mix_buffer.getBuffer();

	std::unique_lock<std::mutex> lock(m_mixingLock);

	while (m_run_mixing_thread)
	{
		/* Get the amount of bytes available for writing. */
		int32_t rb_avail = rb_size - spa_ringbuffer_get_write_index(&m_ringbuffer, &rb_index);
		if (m_fill_ringbuffer && rb_avail > 0) {
			/* As we allocated the ring buffer ourselves, we assume that the samplesize and
			 * the available bytes to read is evenly divisable.
			 */
			int32_t sample_count = rb_avail / samplesize;
			mix(reinterpret_cast<data_t*>(mix_buffer_data), sample_count);
			spa_ringbuffer_write_data(&m_ringbuffer, rb_data, rb_size, rb_index % rb_size, mix_buffer_data, rb_avail);
			rb_index += rb_avail;
			spa_ringbuffer_write_update(&m_ringbuffer, rb_index);
		}
		if (!m_fill_ringbuffer) {
			/* Clear the ringbuffer when we are not playing back to make sure we don't
			 * keep any outdated data.
			 */
			spa_ringbuffer_read_update(&m_ringbuffer, rb_index);
		}
		m_mixingCondition.wait(lock);
	}
}

void PipeWireDevice::mixAudioBuffer(void* device_ptr)
{
	PipeWireDevice* device = (PipeWireDevice*) device_ptr;

	pw_buffer* pw_buf = AUD_pw_stream_dequeue_buffer(device->m_stream);
	if(!pw_buf)
	{
		/* Couldn't get any buffer from PipeWire...*/
		return;
	}

	/* We call this here as the tick is not guaranteed to be up to date
	 * until the "process" callback is triggered.
	 */
	device->m_synchronizer.updateTickStart();

	spa_data& spa_data = pw_buf->buffer->datas[0];
	spa_chunk* chunk = spa_data.chunk;

	chunk->offset = 0;
	chunk->stride = AUD_DEVICE_SAMPLE_SIZE(device->m_specs);
	int n_frames = spa_data.maxsize / chunk->stride;
	if(pw_buf->requested)
	{
		n_frames = SPA_MIN(pw_buf->requested, n_frames);
	}
	chunk->size = n_frames * chunk->stride;

	if(!device->m_fill_ringbuffer)
	{
		/* Queue up silence if we are not queuing up any samples.
		 * If we don't give Pipewire any buffers, it will think we encountered an error.
		 */
		memset(spa_data.data, 0, AUD_FORMAT_SIZE(device->m_specs.format) * chunk->size);
		AUD_pw_stream_queue_buffer(device->m_stream, pw_buf);
		return;
	}
	uint32_t rb_index;
	spa_ringbuffer* ringbuffer = &device->m_ringbuffer;

	int32_t rb_avail = spa_ringbuffer_get_read_index(ringbuffer, &rb_index);
	if (!rb_avail)
	{
		/* Nothing to read from the ring buffer. */
		device->m_mixingCondition.notify_all();
		memset(spa_data.data, 0, AUD_FORMAT_SIZE(device->m_specs.format) * chunk->size);
		AUD_pw_stream_queue_buffer(device->m_stream, pw_buf);
		return;
	}

	/* Here we assume that, if we have available space to read, that the read
	 * buffer size is always enough to fill the output buffer.
	 * This is because the PW_KEY_NODE_LATENCY property that we set should guarantee
	 * that pipewire can't request any bigger buffer sizes than we requested.
	 * (But they can be smaller)
	 */
	uint32_t rb_size = device->m_ringbuffer_data.getSize();
	sample_t* rb_data = device->m_ringbuffer_data.getBuffer();
	spa_ringbuffer_read_data(ringbuffer, rb_data, rb_size, rb_index % rb_size, spa_data.data, chunk->size);
	spa_ringbuffer_read_update(ringbuffer, rb_index + chunk->size);
	device->m_mixingCondition.notify_all();
	AUD_pw_stream_queue_buffer(device->m_stream, pw_buf);
}

void PipeWireDevice::playing(bool playing)
{
	AUD_pw_thread_loop_lock(m_thread);
	AUD_pw_stream_set_active(m_stream, playing);
	AUD_pw_thread_loop_unlock(m_thread);
	m_fill_ringbuffer = playing;
	/* Poke the mixing thread to ensure that it reacts to the m_fill_ringbuffer change. */
	m_mixingCondition.notify_all();
}

PipeWireDevice::PipeWireDevice(const std::string& name, DeviceSpecs specs, int buffersize) :
	m_synchronizer(this),
	m_fill_ringbuffer(false),
	m_run_mixing_thread(true)
{
	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	m_specs = specs;
	spa_audio_format format = SPA_AUDIO_FORMAT_F32;
	switch(m_specs.format)
	{
	case FORMAT_U8:
		format = SPA_AUDIO_FORMAT_U8;
		break;
	case FORMAT_S16:
		format = SPA_AUDIO_FORMAT_S16;
		break;
	case FORMAT_S24:
		format = SPA_AUDIO_FORMAT_S24;
		break;
	case FORMAT_S32:
		format = SPA_AUDIO_FORMAT_S32;
		break;
	case FORMAT_FLOAT32:
		format = SPA_AUDIO_FORMAT_F32;
		break;
	case FORMAT_FLOAT64:
		format = SPA_AUDIO_FORMAT_F64;
		break;
	default:
		break;
	}

	AUD_pw_init(nullptr, nullptr);

	m_thread = AUD_pw_thread_loop_new(name.c_str(), nullptr);
	if(!m_thread)
	{
		AUD_THROW(DeviceException, "Could not create PipeWire thread.");
	}

	m_events = std::make_unique<pw_stream_events>();
	m_events->version = PW_VERSION_STREAM_EVENTS;
	m_events->state_changed = PipeWireDevice::handleStateChanged;
	m_events->process = PipeWireDevice::mixAudioBuffer;

	pw_properties *stream_props = AUD_pw_properties_new(
				PW_KEY_MEDIA_TYPE, "Audio",
				PW_KEY_MEDIA_CATEGORY, "Playback",
				PW_KEY_MEDIA_ROLE, "Production",
				NULL);

	/* Set the requested sample rate and latency. */
	AUD_pw_properties_setf(stream_props, PW_KEY_NODE_RATE, "1/%u", uint(m_specs.rate));
	AUD_pw_properties_setf(stream_props, PW_KEY_NODE_LATENCY, "%u/%u", buffersize, uint(m_specs.rate));

	m_stream = AUD_pw_stream_new_simple(
			AUD_pw_thread_loop_get_loop(m_thread),
			name.c_str(),
			stream_props,
			m_events.get(),
			this);
	if(!m_stream)
	{
		AUD_pw_thread_loop_destroy(m_thread);
		AUD_THROW(DeviceException, "Could not create PipeWire stream.");
	}

	spa_audio_info_raw info{};
	info.channels = m_specs.channels;
	info.format = format;
	info.rate = m_specs.rate;

	uint8_t buffer[1024];
	spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const spa_pod *param = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

	AUD_pw_stream_connect(m_stream,
			  PW_DIRECTION_OUTPUT,
			  PW_ID_ANY,
			  static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
			  PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_INACTIVE |
			  PW_STREAM_FLAG_RT_PROCESS),
			  &param, 1);
	AUD_pw_thread_loop_start(m_thread);

	create();

	spa_ringbuffer_init(&m_ringbuffer);
	m_ringbuffer_data.resize(buffersize * AUD_DEVICE_SAMPLE_SIZE(m_specs));
	m_mixingThread = std::thread(&PipeWireDevice::updateRingBuffers, this);
}

PipeWireDevice::~PipeWireDevice()
{
	/* Ensure that we are not playing back anything anymore. */
	destroy();

	/* Destruct all PipeWire data. */
	AUD_pw_thread_loop_stop(m_thread);
	AUD_pw_stream_destroy(m_stream);
	AUD_pw_thread_loop_destroy(m_thread);
	AUD_pw_deinit();

	{
		/* Ensure that the mixing thread exits. */
		std::unique_lock<std::mutex> lock(m_mixingLock);
		m_run_mixing_thread = false;
		m_mixingCondition.notify_all();
	}
	m_mixingThread.join();
}

ISynchronizer* PipeWireDevice::getSynchronizer()
{
	return &m_synchronizer;
}

class PipeWireDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;
	std::string m_name;

public:
	PipeWireDeviceFactory() : m_buffersize(AUD_DEFAULT_BUFFER_SIZE)
	{
		m_specs.format = FORMAT_S16;
		m_specs.channels = CHANNELS_STEREO;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new PipeWireDevice(m_name, m_specs, m_buffersize));
	}

	virtual int getPriority()
	{
		return 1 << 16;
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
		m_name = name;
	}
};

void PipeWireDevice::registerPlugin()
{
	if(loadPipeWire())
		DeviceManager::registerDevice("PipeWire", std::shared_ptr<IDeviceFactory>(new PipeWireDeviceFactory));
}

#ifdef PIPEWIRE_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	PipeWireDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "Pipewire";
}
#endif

AUD_NAMESPACE_END
