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

#include "AUD_Buffer.h"
#include "AUD_Space.h"

#include <cstring>
#include <cstdlib>

#define AUD_ALIGN(a) (a + 16 - ((long)a & 15))

AUD_Buffer::AUD_Buffer(int size)
{
	m_size = size;
	m_buffer = (data_t*) malloc(size+16);
}

AUD_Buffer::~AUD_Buffer()
{
	free(m_buffer);
}

sample_t* AUD_Buffer::getBuffer() const
{
	return (sample_t*) AUD_ALIGN(m_buffer);
}

int AUD_Buffer::getSize() const
{
	return m_size;
}

void AUD_Buffer::resize(int size, bool keep)
{
	if(keep)
	{
		data_t* buffer = (data_t*) malloc(size + 16);

		memcpy(AUD_ALIGN(buffer), AUD_ALIGN(m_buffer), AUD_MIN(size, m_size));

		free(m_buffer);
		m_buffer = buffer;
	}
	else
		m_buffer = (data_t*) realloc(m_buffer, size + 16);

	m_size = size;
}
