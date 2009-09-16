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

#include "AUD_SDLMixerReader.h"
#include "AUD_Buffer.h"

#include <cstring>

inline Uint16 AUD_TO_SDL(AUD_SampleFormat format)
{
	// SDL only supports 8 and 16 bit audio
	switch(format)
	{
	case AUD_FORMAT_U8:
		return AUDIO_U8;
	case AUD_FORMAT_S16:
		return AUDIO_S16SYS;
	default:
		AUD_THROW(AUD_ERROR_SDL);
	}
}

// greatest common divisor
inline int gcd(int a, int b)
{
	int c;

	// make sure a is the bigger
	if(b > a)
	{
		c = b;
		b = a;
		a = c;
	}

	// greetings from Euclides
	while(b != 0)
	{
		c = a % b;
		a = b;
		b = c;
	}
	return a;
}

AUD_SDLMixerReader::AUD_SDLMixerReader(AUD_IReader* reader,
											 AUD_Specs specs)
{
	if(reader == NULL)
		AUD_THROW(AUD_ERROR_READER);

	m_reader = reader;
	m_tspecs = specs;
	m_sspecs = reader->getSpecs();

	try
	{
		// SDL only supports 8 and 16 bit sample formats
		if(SDL_BuildAudioCVT(&m_cvt,
							 AUD_TO_SDL(m_sspecs.format),
							 m_sspecs.channels,
							 m_sspecs.rate,
							 AUD_TO_SDL(specs.format),
							 specs.channels,
							 specs.rate) == -1)
			AUD_THROW(AUD_ERROR_SDL);
	}
	catch(AUD_Exception)
	{
		delete m_reader; AUD_DELETE("reader")
		throw;
	}

	m_eor = false;
	m_rsposition = 0;
	m_rssize = 0;
	m_ssize = m_sspecs.rate / gcd(specs.rate, m_sspecs.rate);
	m_tsize = m_tspecs.rate * m_ssize / m_sspecs.rate;

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
	m_rsbuffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_SDLMixerReader::~AUD_SDLMixerReader()
{
	delete m_reader; AUD_DELETE("reader")
	delete m_buffer; AUD_DELETE("buffer")
	delete m_rsbuffer; AUD_DELETE("buffer")
}

bool AUD_SDLMixerReader::isSeekable()
{
	return m_reader->isSeekable();
}

void AUD_SDLMixerReader::seek(int position)
{
	m_reader->seek(position * m_ssize / m_tsize);
	m_eor = false;
}

int AUD_SDLMixerReader::getLength()
{
	return m_reader->getLength() * m_tsize / m_ssize;
}

int AUD_SDLMixerReader::getPosition()
{
	return m_reader->getPosition() * m_tsize / m_ssize;
}

AUD_Specs AUD_SDLMixerReader::getSpecs()
{
	return m_tspecs;
}

AUD_ReaderType AUD_SDLMixerReader::getType()
{
	return m_reader->getType();
}

bool AUD_SDLMixerReader::notify(AUD_Message &message)
{
	return m_reader->notify(message);
}

void AUD_SDLMixerReader::read(int & length, sample_t* & buffer)
{
	// sample count for the target buffer without getting a shift
	int tns = length + m_tsize - length % m_tsize;
	// sample count for the source buffer without getting a shift
	int sns = tns * m_ssize / m_tsize;
	// target sample size
	int tss = AUD_SAMPLE_SIZE(m_tspecs);
	// source sample size
	int sss = AUD_SAMPLE_SIZE(m_sspecs);

	// input is output buffer
	int buf_size = AUD_MAX(tns*tss, sns*sss);

	// resize if necessary
	if(m_rsbuffer->getSize() < buf_size)
		m_rsbuffer->resize(buf_size, true);

	if(m_buffer->getSize() < length*tss)
		m_buffer->resize(length*tss);

	buffer = m_buffer->getBuffer();
	int size;
	int index = 0;
	sample_t* buf;

	while(index < length)
	{
		if(m_rsposition == m_rssize)
		{
			// no more data
			if(m_eor)
				length = index;
			// mix
			else
			{
				// read from source
				size = sns;
				m_reader->read(size, buf);

				// prepare
				m_cvt.buf = m_rsbuffer->getBuffer();
				m_cvt.len = size*sss;
				memcpy(m_cvt.buf, buf, size*sss);

				// convert
				SDL_ConvertAudio(&m_cvt);

				// end of reader
				if(size < sns)
					m_eor = true;

				m_rsposition = 0;
				m_rssize = size * m_tsize / m_ssize;
			}
		}

		// size to copy
		size = AUD_MIN(m_rssize-m_rsposition, length-index);

		// copy
		memcpy(m_buffer->getBuffer() + index * tss,
			   m_rsbuffer->getBuffer() + m_rsposition * tss,
			   size*tss);
		m_rsposition += size;
		index += size;
	}
}
