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

#include "AUD_IIRFilterReader.h"

AUD_IIRFilterReader::AUD_IIRFilterReader(AUD_IReader* reader,
										 std::vector<float> b,
										 std::vector<float> a) :
	AUD_BaseIIRFilterReader(reader, b.size(), a.size()), m_a(a), m_b(b)
{
	for(int i = 1; i < m_a.size(); i++)
		m_a[i] /= m_a[0];
	for(int i = 0; i < m_b.size(); i++)
		m_b[i] /= m_a[0];
	m_a[0] = 1;
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
