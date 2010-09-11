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
		m_length(length)
{
}

AUD_FadeType AUD_FaderFactory::getType() const
{
	return m_type;
}

float AUD_FaderFactory::getStart() const
{
	return m_start;
}

float AUD_FaderFactory::getLength() const
{
	return m_length;
}

AUD_IReader* AUD_FaderFactory::createReader() const
{
	return new AUD_FaderReader(getReader(), m_type, m_start, m_length);
}
