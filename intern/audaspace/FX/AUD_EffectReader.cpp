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

#include "AUD_EffectReader.h"

AUD_EffectReader::AUD_EffectReader(AUD_IReader* reader)
{
	if(!reader)
		AUD_THROW(AUD_ERROR_READER);
	m_reader = reader;
}

AUD_EffectReader::~AUD_EffectReader()
{
	delete m_reader; AUD_DELETE("reader")
}

bool AUD_EffectReader::isSeekable()
{
	return m_reader->isSeekable();
}

void AUD_EffectReader::seek(int position)
{
	m_reader->seek(position);
}

int AUD_EffectReader::getLength()
{
	return m_reader->getLength();
}

int AUD_EffectReader::getPosition()
{
	return m_reader->getPosition();
}

AUD_Specs AUD_EffectReader::getSpecs()
{
	return m_reader->getSpecs();
}

AUD_ReaderType AUD_EffectReader::getType()
{
	return m_reader->getType();
}

bool AUD_EffectReader::notify(AUD_Message &message)
{
	return m_reader->notify(message);
}

void AUD_EffectReader::read(int & length, sample_t* & buffer)
{
	m_reader->read(length, buffer);
}
