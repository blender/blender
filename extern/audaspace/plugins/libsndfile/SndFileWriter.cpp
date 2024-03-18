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

#include "SndFileWriter.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

SndFileWriter::SndFileWriter(std::string filename, DeviceSpecs specs,
									 Container format, Codec codec, unsigned int bitrate) :
	m_position(0), m_specs(specs)
{
	SF_INFO sfinfo;

	sfinfo.channels = specs.channels;
	sfinfo.samplerate = int(specs.rate);

	switch(format)
	{
	case CONTAINER_FLAC:
		sfinfo.format = SF_FORMAT_FLAC;
		switch(specs.format)
		{
		case FORMAT_S16:
			sfinfo.format |= SF_FORMAT_PCM_16;
			break;
		case FORMAT_S24:
			sfinfo.format |= SF_FORMAT_PCM_24;
			break;
		case FORMAT_S32:
			sfinfo.format |= SF_FORMAT_PCM_32;
			break;
		case FORMAT_FLOAT32:
			sfinfo.format |= SF_FORMAT_FLOAT;
			break;
		case FORMAT_FLOAT64:
			sfinfo.format |= SF_FORMAT_DOUBLE;
			break;
		default:
			sfinfo.format = 0;
			break;
		}
		break;
	case CONTAINER_OGG:
		if(codec == CODEC_VORBIS)
			sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
		else
			sfinfo.format = 0;
		break;
	case CONTAINER_WAV:
		sfinfo.format = SF_FORMAT_WAV;
		switch(specs.format)
		{
		case FORMAT_U8:
			sfinfo.format |= SF_FORMAT_PCM_U8;
			break;
		case FORMAT_S16:
			sfinfo.format |= SF_FORMAT_PCM_16;
			break;
		case FORMAT_S24:
			sfinfo.format |= SF_FORMAT_PCM_24;
			break;
		case FORMAT_S32:
			sfinfo.format |= SF_FORMAT_PCM_32;
			break;
		case FORMAT_FLOAT32:
			sfinfo.format |= SF_FORMAT_FLOAT;
			break;
		case FORMAT_FLOAT64:
			sfinfo.format |= SF_FORMAT_DOUBLE;
			break;
		default:
			sfinfo.format = 0;
			break;
		}
		break;
	default:
		sfinfo.format = 0;
		break;
	}

	if(sfinfo.format == 0)
		AUD_THROW(FileException, "This format couldn't be written with libsndfile.");

	m_sndfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);

	if(!m_sndfile)
		AUD_THROW(FileException, "The file couldn't be written with libsndfile.");
}

SndFileWriter::~SndFileWriter()
{
	sf_close(m_sndfile);
}

int SndFileWriter::getPosition() const
{
	return m_position;
}

DeviceSpecs SndFileWriter::getSpecs() const
{
	return m_specs;
}

void SndFileWriter::write(unsigned int length, sample_t* buffer)
{
	length = sf_writef_float(m_sndfile, buffer, length);

	m_position += length;
}

AUD_NAMESPACE_END
