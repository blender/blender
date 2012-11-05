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

/** \file audaspace/intern/AUD_FileFactory.cpp
 *  \ingroup audaspaceintern
 */


#ifdef WITH_FFMPEG
// needed for INT64_C
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include "AUD_FFMPEGReader.h"
#endif

#include "AUD_FileFactory.h"

#include <cstring>

#ifdef WITH_SNDFILE
#include "AUD_SndFileReader.h"
#endif

AUD_FileFactory::AUD_FileFactory(std::string filename) :
	m_filename(filename)
{
}

AUD_FileFactory::AUD_FileFactory(const data_t* buffer, int size) :
	m_buffer(new AUD_Buffer(size))
{
	memcpy(m_buffer->getBuffer(), buffer, size);
}

static const char* read_error = "AUD_FileFactory: File couldn't be read.";

boost::shared_ptr<AUD_IReader> AUD_FileFactory::createReader()
{
#ifdef WITH_SNDFILE
	try
	{
		if(m_buffer.get())
			return boost::shared_ptr<AUD_IReader>(new AUD_SndFileReader(m_buffer));
		else
			return boost::shared_ptr<AUD_IReader>(new AUD_SndFileReader(m_filename));
	}
	catch(AUD_Exception&) {}
#endif

#ifdef WITH_FFMPEG
	try
	{
		if(m_buffer.get())
			return boost::shared_ptr<AUD_IReader>(new AUD_FFMPEGReader(m_buffer));
		else
			return boost::shared_ptr<AUD_IReader>(new AUD_FFMPEGReader(m_filename));
	}
	catch(AUD_Exception&) {}
#endif

	AUD_THROW(AUD_ERROR_FILE, read_error);
}
