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

/** \file audaspace/ffmpeg/AUD_FFMPEGFactory.cpp
 *  \ingroup audffmpeg
 */


// needed for INT64_C
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include "AUD_FFMPEGFactory.h"
#include "AUD_FFMPEGReader.h"

AUD_FFMPEGFactory::AUD_FFMPEGFactory(std::string filename) :
		m_filename(filename)
{
}

AUD_FFMPEGFactory::AUD_FFMPEGFactory(const data_t* buffer, int size) :
		m_buffer(new AUD_Buffer(size))
{
	memcpy(m_buffer->getBuffer(), buffer, size);
}

boost::shared_ptr<AUD_IReader> AUD_FFMPEGFactory::createReader()
{
	if(m_buffer.get())
		return boost::shared_ptr<AUD_IReader>(new AUD_FFMPEGReader(m_buffer));
	else
		return boost::shared_ptr<AUD_IReader>(new AUD_FFMPEGReader(m_filename));
}
