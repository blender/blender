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

#include "AUD_FFMPEGFactory.h"
#include "AUD_FFMPEGReader.h"
#include "AUD_Buffer.h"

AUD_FFMPEGFactory::AUD_FFMPEGFactory(const char* filename)
{
	if(filename != NULL)
	{
		m_filename = new char[strlen(filename)+1]; AUD_NEW("string")
		strcpy(m_filename, filename);
	}
	else
		m_filename = NULL;
}

AUD_FFMPEGFactory::AUD_FFMPEGFactory(unsigned char* buffer, int size)
{
	m_filename = NULL;
	m_buffer = AUD_Reference<AUD_Buffer>(new AUD_Buffer(size));
	memcpy(m_buffer.get()->getBuffer(), buffer, size);
}

AUD_FFMPEGFactory::~AUD_FFMPEGFactory()
{
	if(m_filename)
	{
		delete[] m_filename; AUD_DELETE("string")
	}
}

AUD_IReader* AUD_FFMPEGFactory::createReader()
{
	AUD_IReader* reader;
	if(m_filename)
		reader = new AUD_FFMPEGReader(m_filename);
	else
		reader = new AUD_FFMPEGReader(m_buffer);
	AUD_NEW("reader")
	return reader;
}
