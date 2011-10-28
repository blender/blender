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

/** \file audaspace/sndfile/AUD_SndFileWriter.cpp
 *  \ingroup audsndfile
 */


#include "AUD_SndFileWriter.h"

#include <cstring>

static const char* fileopen_error = "AUD_SndFileWriter: File couldn't be written.";
static const char* format_error = "AUD_SndFileWriter: Unsupported format.";

AUD_SndFileWriter::AUD_SndFileWriter(std::string filename, AUD_DeviceSpecs specs,
									 AUD_Container format, AUD_Codec codec, unsigned int bitrate) :
	m_specs(specs)
{
	SF_INFO sfinfo;

	sfinfo.channels = specs.channels;
	sfinfo.samplerate = int(specs.rate);

	switch(format)
	{
	case AUD_CONTAINER_FLAC:
		sfinfo.format = SF_FORMAT_FLAC;
		switch(specs.format)
		{
		case AUD_FORMAT_S16:
			sfinfo.format |= SF_FORMAT_PCM_16;
			break;
		case AUD_FORMAT_S24:
			sfinfo.format |= SF_FORMAT_PCM_24;
			break;
		case AUD_FORMAT_S32:
			sfinfo.format |= SF_FORMAT_PCM_32;
			break;
		case AUD_FORMAT_FLOAT32:
			sfinfo.format |= SF_FORMAT_FLOAT;
			break;
		case AUD_FORMAT_FLOAT64:
			sfinfo.format |= SF_FORMAT_DOUBLE;
			break;
		default:
			sfinfo.format = 0;
			break;
		}
		break;
	case AUD_CONTAINER_OGG:
		if(codec == AUD_CODEC_VORBIS)
			sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
		else
			sfinfo.format = 0;
		break;
	case AUD_CONTAINER_WAV:
		sfinfo.format = SF_FORMAT_WAV;
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			sfinfo.format |= SF_FORMAT_PCM_U8;
			break;
		case AUD_FORMAT_S16:
			sfinfo.format |= SF_FORMAT_PCM_16;
			break;
		case AUD_FORMAT_S24:
			sfinfo.format |= SF_FORMAT_PCM_24;
			break;
		case AUD_FORMAT_S32:
			sfinfo.format |= SF_FORMAT_PCM_32;
			break;
		case AUD_FORMAT_FLOAT32:
			sfinfo.format |= SF_FORMAT_FLOAT;
			break;
		case AUD_FORMAT_FLOAT64:
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
		AUD_THROW(AUD_ERROR_SPECS, format_error);

	m_sndfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);

	if(!m_sndfile)
		AUD_THROW(AUD_ERROR_FILE, fileopen_error);
}

AUD_SndFileWriter::~AUD_SndFileWriter()
{
	sf_close(m_sndfile);
}

int AUD_SndFileWriter::getPosition() const
{
	return m_position;
}

AUD_DeviceSpecs AUD_SndFileWriter::getSpecs() const
{
	return m_specs;
}

void AUD_SndFileWriter::write(unsigned int length, sample_t* buffer)
{
	length = sf_writef_float(m_sndfile, buffer, length);

	m_position += length;
}
