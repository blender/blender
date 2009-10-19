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

#include "AUD_SDLMixer.h"
#include "AUD_SDLMixerFactory.h"

#include <SDL.h>

AUD_SDLMixer::AUD_SDLMixer()
{
	m_factory = NULL;
}

AUD_SDLMixer::~AUD_SDLMixer()
{
	if(m_factory)
	{
		delete m_factory; AUD_DELETE("factory")
	}
}

AUD_IReader* AUD_SDLMixer::prepare(AUD_IReader* reader)
{
	m_factory->setReader(reader);
	return m_factory->createReader();
}

void AUD_SDLMixer::setSpecs(AUD_Specs specs)
{
	m_samplesize = AUD_SAMPLE_SIZE(specs);
	if(m_factory)
	{
		delete m_factory; AUD_DELETE("factory")
	}
	m_factory = new AUD_SDLMixerFactory(specs); AUD_NEW("factory")
}

void AUD_SDLMixer::add(sample_t* buffer, AUD_Specs specs, int length,
					   float volume)
{
	AUD_SDLMixerBuffer buf;
	buf.buffer = buffer;
	buf.length = length;
	buf.volume = volume;
	m_buffers.push_back(buf);
}

void AUD_SDLMixer::superpose(sample_t* buffer, int length, float volume)
{
	AUD_SDLMixerBuffer buf;

	while(!m_buffers.empty())
	{
		buf = m_buffers.front();
		m_buffers.pop_front();
		SDL_MixAudio((Uint8*)buffer,
					 (Uint8*)buf.buffer,
					 buf.length * m_samplesize,
					 (int)(SDL_MIX_MAXVOLUME * volume * buf.volume));
	}
}
