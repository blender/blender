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

#include "SndFileReader.h"
#include "util/Buffer.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

sf_count_t SndFileReader::vio_get_filelen(void* user_data)
{
	SndFileReader* reader = (SndFileReader*)user_data;
	return reader->m_membuffer->getSize();
}

sf_count_t SndFileReader::vio_seek(sf_count_t offset, int whence,
									   void* user_data)
{
	SndFileReader* reader = (SndFileReader*)user_data;

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

sf_count_t SndFileReader::vio_read(void* ptr, sf_count_t count,
									   void* user_data)
{
	SndFileReader* reader = (SndFileReader*)user_data;

	if(reader->m_memoffset + count > reader->m_membuffer->getSize())
		count = reader->m_membuffer->getSize() - reader->m_memoffset;

	std::memcpy(ptr, ((data_t*)reader->m_membuffer->getBuffer()) +
		   reader->m_memoffset, count);
	reader->m_memoffset += count;

	return count;
}

sf_count_t SndFileReader::vio_tell(void* user_data)
{
	SndFileReader* reader = (SndFileReader*)user_data;

	return reader->m_memoffset;
}

SndFileReader::SndFileReader(std::string filename) :
	m_position(0)
{
	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);

	if(!m_sndfile)
		AUD_THROW(FileException, "The file couldn't be opened with libsndfile.");

	m_specs.channels = (Channels) sfinfo.channels;
	m_specs.rate = (SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
}

SndFileReader::SndFileReader(std::shared_ptr<Buffer> buffer) :
	m_position(0),
	m_membuffer(buffer),
	m_memoffset(0)
{
	m_vio.get_filelen = vio_get_filelen;
	m_vio.read = vio_read;
	m_vio.seek = vio_seek;
	m_vio.tell = vio_tell;
	m_vio.write = nullptr;

	SF_INFO sfinfo;

	sfinfo.format = 0;
	m_sndfile = sf_open_virtual(&m_vio, SFM_READ, &sfinfo, this);

	if(!m_sndfile)
		AUD_THROW(FileException, "The buffer couldn't be read with libsndfile.");

	m_specs.channels = (Channels) sfinfo.channels;
	m_specs.rate = (SampleRate) sfinfo.samplerate;
	m_length = sfinfo.frames;
	m_seekable = sfinfo.seekable;
}

SndFileReader::~SndFileReader()
{
	sf_close(m_sndfile);
}

std::vector<StreamInfo> SndFileReader::queryStreams()
{
	std::vector<StreamInfo> result;

	StreamInfo info;
	info.start = 0;
	info.duration = double(getLength()) / m_specs.rate;
	info.specs.specs = m_specs;
	info.specs.format = FORMAT_FLOAT32;

	result.emplace_back(info);

	return result;
}

bool SndFileReader::isSeekable() const
{
	return m_seekable;
}

void SndFileReader::seek(int position)
{
	if(m_seekable)
	{
		position = sf_seek(m_sndfile, position, SEEK_SET);
		m_position = position;
	}
}

int SndFileReader::getLength() const
{
	return m_length;
}

int SndFileReader::getPosition() const
{
	return m_position;
}

Specs SndFileReader::getSpecs() const
{
	return m_specs;
}

void SndFileReader::read(int& length, bool& eos, sample_t* buffer)
{
	int olen = length;

	length = sf_readf_float(m_sndfile, buffer, length);

	m_position += length;

	eos = length < olen;
}

AUD_NAMESPACE_END
