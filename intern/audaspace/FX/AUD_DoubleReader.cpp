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

/** \file audaspace/FX/AUD_DoubleReader.cpp
 *  \ingroup audfx
 */


#include "AUD_DoubleReader.h"

#include <cstring>

AUD_DoubleReader::AUD_DoubleReader(boost::shared_ptr<AUD_IReader> reader1,
								   boost::shared_ptr<AUD_IReader> reader2) :
		m_reader1(reader1), m_reader2(reader2), m_finished1(false)
{
	AUD_Specs s1, s2;
	s1 = reader1->getSpecs();
	s2 = reader2->getSpecs();
}

AUD_DoubleReader::~AUD_DoubleReader()
{
}

bool AUD_DoubleReader::isSeekable() const
{
	return m_reader1->isSeekable() && m_reader2->isSeekable();
}

void AUD_DoubleReader::seek(int position)
{
	m_reader1->seek(position);

	int pos1 = m_reader1->getPosition();

	if((m_finished1 = (pos1 < position)))
		m_reader2->seek(position - pos1);
	else
		m_reader2->seek(0);
}

int AUD_DoubleReader::getLength() const
{
	int len1 = m_reader1->getLength();
	int len2 = m_reader2->getLength();
	if(len1 < 0 || len2 < 0)
		return -1;
	return len1 + len2;
}

int AUD_DoubleReader::getPosition() const
{
	return m_reader1->getPosition() + m_reader2->getPosition();
}

AUD_Specs AUD_DoubleReader::getSpecs() const
{
	return m_finished1 ? m_reader1->getSpecs() : m_reader2->getSpecs();
}

void AUD_DoubleReader::read(int& length, bool& eos, sample_t* buffer)
{
	eos = false;

	if(!m_finished1)
	{
		int len = length;

		m_reader1->read(len, m_finished1, buffer);

		if(len < length)
		{
			AUD_Specs specs1, specs2;
			specs1 = m_reader1->getSpecs();
			specs2 = m_reader2->getSpecs();
			if(AUD_COMPARE_SPECS(specs1, specs2))
			{
				int len2 = length - len;
				m_reader2->read(len2, eos, buffer + specs1.channels * len);
				length = len + len2;
			}
			else
				length = len;
		}
	}
	else
	{
		m_reader2->read(length, eos, buffer);
	}
}
