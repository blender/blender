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

#include "AUD_SndFileReader.h"
#include "AUD_Buffer.h"

#include <cstring>

sf_count_t AUD_SndFileReader::vio_get_filelen(void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;
	return reader->m_membuffer.get()->getSize();
}

sf_count_t AUD_SndFileReader::vio_seek(sf_count_t offset, int whence,
									   void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;

	switch(whence)
	{
	case SEEK_SET:
		reader->m_memoffset = offset;
		break;
	case SEEK_CUR:
		reader->m_memoffset = reader->m_memoffset + offset;
		break;
	case SEEK_END:
		reader->m_memoffset = reader->m_membuffer.get()->getSize() + offset;
		break;
	}

	return reader->m_memoffset;
}

sf_count_t AUD_SndFileReader::vio_read(void *ptr, sf_count_t count,
									   void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;

	if(reader->m_memoffset + count > reader->m_membuffer.get()->getSize())
		count = reader->m_membuffer.get()->getSize() - reader->m_memoffset;

	memcpy(ptr, ((data_t*)reader->m_membuffer.get()->getBuffer()) +
		   reader->m_memoffset, count);
	reader->m_memoffset += count;

	return count;
}

sf_count_t AUD_SndFileReader::vio_tell(void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;

	return reader->m_memoffset;
}

AUD_SndFileReader::AUD_SndFileReader(const char* filename)
{
	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open(filename, SFM_READ, &sfinfo);

	if(!m_sndfile)
		AUD_THROW(AUD_ERROR_FILE);

	m_specs.channels = (AUD_Channels) sfinfo.channels;
	m_specs.rate = (AUD_SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
	m_position = 0;

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_SndFileReader::AUD_SndFileReader(AUD_Reference<AUD_Buffer> buffer)
{
	m_membuffer = buffer;
	m_memoffset = 0;

	m_vio.get_filelen = vio_get_filelen;
	m_vio.read = vio_read;
	m_vio.seek = vio_seek;
	m_vio.tell = vio_tell;
	m_vio.write = NULL;

	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open_virtual(&m_vio, SFM_READ, &sfinfo, this);

	if(!m_sndfile)
		AUD_THROW(AUD_ERROR_FILE);

	m_specs.channels = (AUD_Channels) sfinfo.channels;
	m_specs.rate = (AUD_SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
	m_position = 0;

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_SndFileReader::~AUD_SndFileReader()
{
	sf_close(m_sndfile);

	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_SndFileReader::isSeekable()
{
	return m_seekable;
}

void AUD_SndFileReader::seek(int position)
{
	if(m_seekable)
	{
		position = sf_seek(m_sndfile, position, SEEK_SET);
		m_position = position;
	}
}

int AUD_SndFileReader::getLength()
{
	return m_length;
}

int AUD_SndFileReader::getPosition()
{
	return m_position;
}

AUD_Specs AUD_SndFileReader::getSpecs()
{
	return m_specs;
}

AUD_ReaderType AUD_SndFileReader::getType()
{
	return AUD_TYPE_STREAM;
}

bool AUD_SndFileReader::notify(AUD_Message &message)
{
	return false;
}

void AUD_SndFileReader::read(int & length, sample_t* & buffer)
{
	int sample_size = AUD_SAMPLE_SIZE(m_specs);

	// resize output buffer if necessary
	if(m_buffer->getSize() < length*sample_size)
		m_buffer->resize(length*sample_size);

	buffer = m_buffer->getBuffer();

	length = sf_readf_float(m_sndfile, buffer, length);

	m_position += length;
}
