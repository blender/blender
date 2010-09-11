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

#include "AUD_SndFileFactory.h"
#include "AUD_SndFileReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_SndFileFactory::AUD_SndFileFactory(std::string filename) :
	m_filename(filename)
{
}

AUD_SndFileFactory::AUD_SndFileFactory(const data_t* buffer, int size) :
	m_buffer(new AUD_Buffer(size))
{
	memcpy(m_buffer.get()->getBuffer(), buffer, size);
}

AUD_IReader* AUD_SndFileFactory::createReader() const
{
	if(m_buffer.get())
		return new AUD_SndFileReader(m_buffer);
	else
		return new AUD_SndFileReader(m_filename);
}
