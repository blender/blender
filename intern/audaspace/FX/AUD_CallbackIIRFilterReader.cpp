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

#include "AUD_CallbackIIRFilterReader.h"

AUD_CallbackIIRFilterReader::AUD_CallbackIIRFilterReader(AUD_IReader* reader,
														 int in, int out,
														 doFilterIIR doFilter,
														 endFilterIIR endFilter,
														 void* data) :
	AUD_BaseIIRFilterReader(reader, in, out),
	m_filter(doFilter), m_endFilter(endFilter), m_data(data)
{
}

AUD_CallbackIIRFilterReader::~AUD_CallbackIIRFilterReader()
{
	if(m_endFilter)
		m_endFilter(m_data);
}

sample_t AUD_CallbackIIRFilterReader::filter()
{
	return m_filter(this, m_data);
}
