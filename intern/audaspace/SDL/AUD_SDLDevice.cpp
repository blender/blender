/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/SDL/AUD_SDLDevice.cpp
 *  \ingroup audsdl
 */


#include "AUD_SDLDevice.h"
#include "AUD_IReader.h"

void AUD_SDLDevice::SDL_mix(void *data, Uint8* buffer, int length)
{
	AUD_SDLDevice* device = (AUD_SDLDevice*)data;

	device->mix((data_t*)buffer,length/AUD_DEVICE_SAMPLE_SIZE(device->m_specs));
}

static const char* open_error = "AUD_SDLDevice: Device couldn't be opened.";
static const char* format_error = "AUD_SDLDevice: Obtained unsupported sample "
								  "format.";

AUD_SDLDevice::AUD_SDLDevice(AUD_DeviceSpecs specs, int buffersize)
{
	if(specs.channels == AUD_CHANNELS_INVALID)
		specs.channels = AUD_CHANNELS_STEREO;
	if(specs.format == AUD_FORMAT_INVALID)
		specs.format = AUD_FORMAT_S16;
	if(specs.rate == AUD_RATE_INVALID)
		specs.rate = AUD_RATE_44100;

	m_specs = specs;

	SDL_AudioSpec format, obtained;

	format.freq = m_specs.rate;
	if(m_specs.format == AUD_FORMAT_U8)
		format.format = AUDIO_U8;
	else
		format.format = AUDIO_S16SYS;
	format.channels = m_specs.channels;
	format.samples = buffersize;
	format.callback = AUD_SDLDevice::SDL_mix;
	format.userdata = this;

	if(SDL_OpenAudio(&format, &obtained) != 0)
		AUD_THROW(AUD_ERROR_SDL, open_error);

	m_specs.rate = (AUD_SampleRate)obtained.freq;
	m_specs.channels = (AUD_Channels)obtained.channels;
	if(obtained.format == AUDIO_U8)
		m_specs.format = AUD_FORMAT_U8;
	else if(obtained.format == AUDIO_S16LSB || obtained.format == AUDIO_S16MSB)
		m_specs.format = AUD_FORMAT_S16;
	else
	{
		SDL_CloseAudio();
		AUD_THROW(AUD_ERROR_SDL, format_error);
	}

	create();
}

AUD_SDLDevice::~AUD_SDLDevice()
{
	lock();
	SDL_CloseAudio();
	unlock();

	destroy();
}

void AUD_SDLDevice::playing(bool playing)
{
	SDL_PauseAudio(playing ? 0 : 1);
}
