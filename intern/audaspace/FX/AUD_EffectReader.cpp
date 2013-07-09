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

/** \file audaspace/FX/AUD_EffectReader.cpp
 *  \ingroup audfx
 */


#include "AUD_EffectReader.h"

AUD_EffectReader::AUD_EffectReader(boost::shared_ptr<AUD_IReader> reader)
{
	m_reader = reader;
}

AUD_EffectReader::~AUD_EffectReader()
{
}

bool AUD_EffectReader::isSeekable() const
{
	return m_reader->isSeekable();
}

void AUD_EffectReader::seek(int position)
{
	m_reader->seek(position);
}

int AUD_EffectReader::getLength() const
{
	return m_reader->getLength();
}

int AUD_EffectReader::getPosition() const
{
	return m_reader->getPosition();
}

AUD_Specs AUD_EffectReader::getSpecs() const
{
	return m_reader->getSpecs();
}

void AUD_EffectReader::read(int& length, bool& eos, sample_t* buffer)
{
	m_reader->read(length, eos, buffer);
}
