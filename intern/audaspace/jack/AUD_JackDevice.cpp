/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#include "AUD_Mixer.h"
#include "AUD_JackDevice.h"
#include "AUD_IReader.h"
#include "AUD_Buffer.h"

#include <stdio.h>
#include <stdlib.h>

void* AUD_JackDevice::runMixingThread(void* device)
{
	((AUD_JackDevice*)device)->updateRingBuffers();
	return NULL;
}

void AUD_JackDevice::updateRingBuffers()
{
	size_t size, temp;
	unsigned int samplesize = AUD_SAMPLE_SIZE(m_specs);
	unsigned int i, j;
	unsigned int channels = m_specs.channels;
	sample_t* buffer = m_buffer->getBuffer();
	float* deinterleave = m_deinterleavebuf->getBuffer();
	jack_transport_state_t state;
	jack_position_t position;

	pthread_mutex_lock(&m_mixingLock);
	while(m_valid)
	{
		if(m_sync > 1)
		{
			if(m_syncFunc)
			{
				state = jack_transport_query(m_client, &position);
				m_syncFunc(m_syncFuncData, state != JackTransportStopped, position.frame / (float) m_specs.rate);
			}

			for(i = 0; i < channels; i++)
				jack_ringbuffer_reset(m_ringbuffers[i]);
		}

		size = jack_ringbuffer_write_space(m_ringbuffers[0]);
		for(i = 1; i < channels; i++)
			if((temp = jack_ringbuffer_write_space(m_ringbuffers[i])) < size)
				size = temp;

		while(size > samplesize)
		{
			size /= samplesize;
			mix((data_t*)buffer, size);
			for(i = 0; i < channels; i++)
			{
				for(j = 0; j < size; j++)
					deinterleave[i * size + j] = buffer[i + j * channels];
				jack_ringbuffer_write(m_ringbuffers[i], (char*)(deinterleave + i * size), size * sizeof(float));
			}

			size = jack_ringbuffer_write_space(m_ringbuffers[0]);
			for(i = 1; i < channels; i++)
				if((temp = jack_ringbuffer_write_space(m_ringbuffers[i])) < size)
					size = temp;
		}

		if(m_sync > 1)
		{
			m_sync = 3;
		}

		pthread_cond_wait(&m_mixingCondition, &m_mixingLock);
	}
	pthread_mutex_unlock(&m_mixingLock);
}

int AUD_JackDevice::jack_mix(jack_nframes_t length, void *data)
{
	AUD_JackDevice* device = (AUD_JackDevice*)data;
	unsigned int i;
	int count = device->m_specs.channels;
	char* buffer;

	if(device->m_sync)
	{
		// play silence while syncing
		for(unsigned int i = 0; i < count; i++)
			memset(jack_port_get_buffer(device->m_ports[i], length), 0, length * sizeof(float));
	}
	else
	{
		size_t temp;
		size_t readsamples = jack_ringbuffer_read_space(device->m_ringbuffers[0]);
		for(i = 1; i < count; i++)
			if((temp = jack_ringbuffer_read_space(device->m_ringbuffers[i])) < readsamples)
				readsamples = temp;

		readsamples = AUD_MIN(readsamples / sizeof(float), length);

		for(unsigned int i = 0; i < count; i++)
		{
			buffer = (char*)jack_port_get_buffer(device->m_ports[i], length);
			jack_ringbuffer_read(device->m_ringbuffers[i], buffer, readsamples * sizeof(float));
			if(readsamples < length)
				memset(buffer + readsamples * sizeof(float), 0, (length - readsamples) * sizeof(float));
		}

		if(pthread_mutex_trylock(&(device->m_mixingLock)) == 0)
		{
			pthread_cond_signal(&(device->m_mixingCondition));
			pthread_mutex_unlock(&(device->m_mixingLock));
		}
	}

	return 0;
}

int AUD_JackDevice::jack_sync(jack_transport_state_t state, jack_position_t* pos, void* data)
{
	AUD_JackDevice* device = (AUD_JackDevice*)data;

	if(state == JackTransportStopped)
		return 1;

	if(pthread_mutex_trylock(&(device->m_mixingLock)) == 0)
	{
		if(device->m_sync > 2)
		{
			if(device->m_sync == 3)
			{
				device->m_sync = 0;
				pthread_mutex_unlock(&(device->m_mixingLock));
				return 1;
			}
		}
		else
		{
			device->m_sync = 2;
			pthread_cond_signal(&(device->m_mixingCondition));
		}
		pthread_mutex_unlock(&(device->m_mixingLock));
	}
	else if(!device->m_sync)
		device->m_sync = 1;

	return 0;
}

void AUD_JackDevice::jack_shutdown(void *data)
{
	AUD_JackDevice* device = (AUD_JackDevice*)data;
	device->m_valid = false;
}

AUD_JackDevice::AUD_JackDevice(AUD_DeviceSpecs specs, int buffersize)
{
	if(specs.channels == AUD_CHANNELS_INVALID)
		specs.channels = AUD_CHANNELS_STEREO;

	// jack uses floats
	m_specs = specs;
	m_specs.format = AUD_FORMAT_FLOAT32;

	jack_options_t options = JackNullOption;
	jack_status_t status;

	// open client
	m_client = jack_client_open("Blender", options, &status);
	if(m_client == NULL)
		AUD_THROW(AUD_ERROR_JACK);

	// set callbacks
	jack_set_process_callback(m_client, AUD_JackDevice::jack_mix, this);
	jack_on_shutdown(m_client, AUD_JackDevice::jack_shutdown, this);
	jack_set_sync_callback(m_client, AUD_JackDevice::jack_sync, this);

	// register our output channels which are called ports in jack
	m_ports = new jack_port_t*[m_specs.channels]; AUD_NEW("jack_port")

	try
	{
		char portname[64];
		for(int i = 0; i < m_specs.channels; i++)
		{
			sprintf(portname, "out %d", i+1);
			m_ports[i] = jack_port_register(m_client, portname,
											JACK_DEFAULT_AUDIO_TYPE,
											JackPortIsOutput, 0);
			if(m_ports[i] == NULL)
				AUD_THROW(AUD_ERROR_JACK);
		}
	}
	catch(AUD_Exception)
	{
		jack_client_close(m_client);
		delete[] m_ports; AUD_DELETE("jack_port")
		throw;
	}

	m_specs.rate = (AUD_SampleRate)jack_get_sample_rate(m_client);

	buffersize *= sizeof(sample_t);
	m_ringbuffers = new jack_ringbuffer_t*[specs.channels]; AUD_NEW("jack_buffers")
	for(unsigned int i = 0; i < specs.channels; i++)
		m_ringbuffers[i] = jack_ringbuffer_create(buffersize);
	buffersize *= specs.channels;
	m_buffer = new AUD_Buffer(buffersize); AUD_NEW("buffer");
	m_deinterleavebuf = new AUD_Buffer(buffersize); AUD_NEW("buffer");

	create();

	m_valid = true;
	m_playing = false;
	m_sync = 0;
	m_syncFunc = NULL;

	pthread_mutex_init(&m_mixingLock, NULL);
	pthread_cond_init(&m_mixingCondition, NULL);

	try
	{
		// activate the client
		if(jack_activate(m_client))
			AUD_THROW(AUD_ERROR_JACK);
	}
	catch(AUD_Exception)
	{
		jack_client_close(m_client);
		delete[] m_ports; AUD_DELETE("jack_port")
		delete m_buffer; AUD_DELETE("buffer");
		delete m_deinterleavebuf; AUD_DELETE("buffer");
		for(unsigned int i = 0; i < specs.channels; i++)
			jack_ringbuffer_free(m_ringbuffers[i]);
		delete[] m_ringbuffers; AUD_DELETE("jack_buffers")
		pthread_mutex_destroy(&m_mixingLock);
		pthread_cond_destroy(&m_mixingCondition);
		destroy();
		throw;
	}

	const char** ports = jack_get_ports(m_client, NULL, NULL,
										JackPortIsPhysical | JackPortIsInput);
	if(ports != NULL)
	{
		for(int i = 0; i < m_specs.channels && ports[i]; i++)
			jack_connect(m_client, jack_port_name(m_ports[i]), ports[i]);

		free(ports);
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_create(&m_mixingThread, &attr, runMixingThread, this);

	pthread_attr_destroy(&attr);
}

AUD_JackDevice::~AUD_JackDevice()
{
	if(m_valid)
		jack_client_close(m_client);
	m_valid = false;

	delete[] m_ports; AUD_DELETE("jack_port")

	pthread_mutex_lock(&m_mixingLock);
	pthread_cond_signal(&m_mixingCondition);
	pthread_mutex_unlock(&m_mixingLock);
	pthread_join(m_mixingThread, NULL);

	pthread_cond_destroy(&m_mixingCondition);
	pthread_mutex_destroy(&m_mixingLock);
	delete m_buffer; AUD_DELETE("buffer");
	delete m_deinterleavebuf; AUD_DELETE("buffer");
	for(unsigned int i = 0; i < m_specs.channels; i++)
		jack_ringbuffer_free(m_ringbuffers[i]);
	delete[] m_ringbuffers; AUD_DELETE("jack_buffers")

	destroy();
}

void AUD_JackDevice::playing(bool playing)
{
	// Do nothing.
}

void AUD_JackDevice::startPlayback()
{
	jack_transport_start(m_client);
}

void AUD_JackDevice::stopPlayback()
{
	jack_transport_stop(m_client);
}

void AUD_JackDevice::seekPlayback(float time)
{
	if(time >= 0.0f)
		jack_transport_locate(m_client, time * m_specs.rate);
}

void AUD_JackDevice::setSyncCallback(AUD_syncFunction sync, void* data)
{
	m_syncFunc = sync;
	m_syncFuncData = data;
}

float AUD_JackDevice::getPlaybackPosition()
{
	jack_position_t position;
	jack_transport_query(m_client, &position);
	return position.frame / (float) m_specs.rate;
}

bool AUD_JackDevice::doesPlayback()
{
	return jack_transport_query(m_client, NULL) != JackTransportStopped;
}
