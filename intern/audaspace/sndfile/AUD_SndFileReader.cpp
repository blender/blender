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

/** \file audaspace/sndfile/AUD_SndFileReader.cpp
 *  \ingroup audsndfile
 */


#include "AUD_SndFileReader.h"

#include <cstring>

sf_count_t AUD_SndFileReader::vio_get_filelen(void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;
	return reader->m_membuffer->getSize();
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
		reader->m_memoffset = reader->m_membuffer->getSize() + offset;
		break;
	}

	return reader->m_memoffset;
}

sf_count_t AUD_SndFileReader::vio_read(void *ptr, sf_count_t count,
									   void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;

	if(reader->m_memoffset + count > reader->m_membuffer->getSize())
		count = reader->m_membuffer->getSize() - reader->m_memoffset;

	memcpy(ptr, ((data_t*)reader->m_membuffer->getBuffer()) +
		   reader->m_memoffset, count);
	reader->m_memoffset += count;

	return count;
}

sf_count_t AUD_SndFileReader::vio_tell(void *user_data)
{
	AUD_SndFileReader* reader = (AUD_SndFileReader*)user_data;

	return reader->m_memoffset;
}

static const char* fileopen_error = "AUD_SndFileReader: File couldn't be "
									"read.";

AUD_SndFileReader::AUD_SndFileReader(std::string filename) :
	m_position(0)
{
	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);

	if(!m_sndfile)
		AUD_THROW(AUD_ERROR_FILE, fileopen_error);

	m_specs.channels = (AUD_Channels) sfinfo.channels;
	m_specs.rate = (AUD_SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
}

AUD_SndFileReader::AUD_SndFileReader(boost::shared_ptr<AUD_Buffer> buffer) :
	m_position(0),
	m_membuffer(buffer),
	m_memoffset(0)
{
	m_vio.get_filelen = vio_get_filelen;
	m_vio.read = vio_read;
	m_vio.seek = vio_seek;
	m_vio.tell = vio_tell;
	m_vio.write = NULL;

	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open_virtual(&m_vio, SFM_READ, &sfinfo, this);

	if(!m_sndfile)
		AUD_THROW(AUD_ERROR_FILE, fileopen_error);

	m_specs.channels = (AUD_Channels) sfinfo.channels;
	m_specs.rate = (AUD_SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
}

AUD_SndFileReader::~AUD_SndFileReader()
{
	sf_close(m_sndfile);
}

bool AUD_SndFileReader::isSeekable() const
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

int AUD_SndFileReader::getLength() const
{
	return m_length;
}

int AUD_SndFileReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_SndFileReader::getSpecs() const
{
	return m_specs;
}

void AUD_SndFileReader::read(int& length, bool& eos, sample_t* buffer)
{
	int olen = length;

	length = sf_readf_float(m_sndfile, buffer, length);

	m_position += length;

	eos = length < olen;
}
