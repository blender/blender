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

// AUD_XXX this is not realtime suitable!
int AUD_JackDevice::jack_mix(jack_nframes_t length, void *data)
{
	AUD_JackDevice* device = (AUD_JackDevice*)data;
	unsigned int samplesize = AUD_SAMPLE_SIZE(device->m_specs);
	if(device->m_buffer->getSize() < samplesize * length)
		device->m_buffer->resize(samplesize * length);
	device->mix((data_t*)device->m_buffer->getBuffer(), length);

	float* in = (float*) device->m_buffer->getBuffer();
	float* out;
	int count = device->m_specs.channels;

	for(unsigned int i = 0; i < count; i++)
	{
		out = (float*)jack_port_get_buffer(device->m_ports[i], length);
		for(unsigned int j = 0; j < length; j++)
			out[j] = in[j * count + i];
	}

	return 0;
}

void AUD_JackDevice::jack_shutdown(void *data)
{
	AUD_JackDevice* device = (AUD_JackDevice*)data;
	device->m_valid = false;
}

AUD_JackDevice::AUD_JackDevice(AUD_DeviceSpecs specs)
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

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer");

	// set callbacks
	jack_set_process_callback(m_client, AUD_JackDevice::jack_mix, this);
	jack_on_shutdown(m_client, AUD_JackDevice::jack_shutdown, this);

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

		m_specs.rate = (AUD_SampleRate)jack_get_sample_rate(m_client);

		// activate the client
		if(jack_activate(m_client))
			AUD_THROW(AUD_ERROR_JACK);
	}
	catch(AUD_Exception)
	{
		jack_client_close(m_client);
		delete[] m_ports; AUD_DELETE("jack_port")
		delete m_buffer; AUD_DELETE("buffer");
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

	m_valid = true;

	create();
}

AUD_JackDevice::~AUD_JackDevice()
{
	lock();
	if(m_valid)
		jack_client_close(m_client);
	delete[] m_ports; AUD_DELETE("jack_port")
	delete m_buffer; AUD_DELETE("buffer");
	unlock();

	destroy();
}

void AUD_JackDevice::playing(bool playing)
{
	// Do nothing.
}
