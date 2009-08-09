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

#include "AUD_LimiterFactory.h"
#include "AUD_LimiterReader.h"
#include "AUD_Space.h"

AUD_LimiterFactory::AUD_LimiterFactory(AUD_IFactory* factory,
									   float start, float end) :
		AUD_EffectFactory(factory),
		m_start(start),
		m_end(end) {}

float AUD_LimiterFactory::getStart()
{
	return m_start;
}

void AUD_LimiterFactory::setStart(float start)
{
	m_start = start;
}

float AUD_LimiterFactory::getEnd()
{
	return m_end;
}

void AUD_LimiterFactory::setEnd(float end)
{
	m_end = end;
}

AUD_IReader* AUD_LimiterFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_LimiterReader(reader, m_start, m_end);
		AUD_NEW("reader")
	}

	return reader;
}
