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

#include "AUD_FaderFactory.h"
#include "AUD_FaderReader.h"

AUD_FaderFactory::AUD_FaderFactory(AUD_IFactory* factory, AUD_FadeType type,
								   float start, float length) :
		AUD_EffectFactory(factory),
		m_type(type),
		m_start(start),
		m_length(length) {}

AUD_FaderFactory::AUD_FaderFactory(AUD_FadeType type,
								   float start, float length) :
		AUD_EffectFactory(0),
		m_type(type),
		m_start(start),
		m_length(length) {}

AUD_FadeType AUD_FaderFactory::getType()
{
	return m_type;
}

void AUD_FaderFactory::setType(AUD_FadeType type)
{
	m_type = type;
}

float AUD_FaderFactory::getStart()
{
	return m_start;
}

void AUD_FaderFactory::setStart(float start)
{
	m_start = start;
}

float AUD_FaderFactory::getLength()
{
	return m_length;
}

void AUD_FaderFactory::setLength(float length)
{
	m_length = length;
}

AUD_IReader* AUD_FaderFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_FaderReader(reader, m_type, m_start, m_length);
		AUD_NEW("reader")
	}

	return reader;
}
