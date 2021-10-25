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

/** \file audaspace/FX/AUD_IIRFilterReader.cpp
 *  \ingroup audfx
 */


#include "AUD_IIRFilterReader.h"

AUD_IIRFilterReader::AUD_IIRFilterReader(boost::shared_ptr<AUD_IReader> reader,
										 const std::vector<float>& b,
										 const std::vector<float>& a) :
	AUD_BaseIIRFilterReader(reader, b.size(), a.size()), m_a(a), m_b(b)
{
	if(m_a.empty() == false)
	{
		for(int i = 1; i < m_a.size(); i++)
			m_a[i] /= m_a[0];
		for(int i = 0; i < m_b.size(); i++)
			m_b[i] /= m_a[0];
		m_a[0] = 1;
	}
}

sample_t AUD_IIRFilterReader::filter()
{
	sample_t out = 0;

	for(int i = 1; i < m_a.size(); i++)
		out -= y(-i) * m_a[i];
	for(int i = 0; i < m_b.size(); i++)
		out += x(-i) * m_b[i];

	return out;
}

void AUD_IIRFilterReader::setCoefficients(const std::vector<float>& b,
										  const std::vector<float>& a)
{
	setLengths(b.size(), a.size());
	m_a = a;
	m_b = b;
}
