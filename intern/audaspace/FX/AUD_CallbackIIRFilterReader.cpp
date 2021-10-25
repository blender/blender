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

/** \file audaspace/FX/AUD_CallbackIIRFilterReader.cpp
 *  \ingroup audfx
 */


#include "AUD_CallbackIIRFilterReader.h"

AUD_CallbackIIRFilterReader::AUD_CallbackIIRFilterReader(boost::shared_ptr<AUD_IReader> reader,
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
