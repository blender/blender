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

/** \file audaspace/intern/AUD_FileWriter.cpp
 *  \ingroup audaspaceintern
 */

#ifdef WITH_FFMPEG
// needed for INT64_C
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include "AUD_FFMPEGWriter.h"
#endif

#ifdef WITH_SNDFILE
#include "AUD_SndFileWriter.h"
#endif

#include "AUD_FileWriter.h"
#include "AUD_Buffer.h"

static const char* write_error = "AUD_FileWriter: File couldn't be written.";

AUD_Reference<AUD_IWriter> AUD_FileWriter::createWriter(std::string filename,AUD_DeviceSpecs specs,
														AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
#ifdef WITH_SNDFILE
	try
	{
		return new AUD_SndFileWriter(filename, specs, format, codec, bitrate);
	}
	catch(AUD_Exception&) {}
#endif

#ifdef WITH_FFMPEG
	try
	{
		return new AUD_FFMPEGWriter(filename, specs, format, codec, bitrate);
	}
	catch(AUD_Exception&) {}
#endif

	AUD_THROW(AUD_ERROR_SPECS, write_error);
}

void AUD_FileWriter::writeReader(AUD_Reference<AUD_IReader> reader, AUD_Reference<AUD_IWriter> writer, unsigned int length, unsigned int buffersize)
{
	AUD_Buffer buffer(buffersize * AUD_SAMPLE_SIZE(writer->getSpecs()));
	sample_t* buf = buffer.getBuffer();

	int len;
	bool eos = false;
	int channels = writer->getSpecs().channels;

	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int i = 0; i < len * channels; i++)
		{
			// clamping!
			if(buf[i] > 1)
				buf[i] = 1;
			else if(buf[i] < -1)
				buf[i] = -1;
		}

		writer->write(len, buf);
	}
}

void AUD_FileWriter::writeReader(AUD_Reference<AUD_IReader> reader, std::vector<AUD_Reference<AUD_IWriter> >& writers, unsigned int length, unsigned int buffersize)
{
	AUD_Buffer buffer(buffersize * AUD_SAMPLE_SIZE(reader->getSpecs()));
	AUD_Buffer buffer2(buffersize * sizeof(sample_t));
	sample_t* buf = buffer.getBuffer();
	sample_t* buf2 = buffer2.getBuffer();

	int len;
	bool eos = false;
	int channels = reader->getSpecs().channels;

	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int channel = 0; channel < channels; channel++)
		{
			for(int i = 0; i < len; i++)
			{
				// clamping!
				if(buf[i * channels + channel] > 1)
					buf2[i] = 1;
				else if(buf[i * channels + channel] < -1)
					buf2[i] = -1;
				else
					buf2[i] = buf[i * channels + channel];
			}

			writers[channel]->write(len, buf2);
		}
	}
}
