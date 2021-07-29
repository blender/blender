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

/** \file audaspace/FX/AUD_FaderFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_FaderFactory.h"
#include "AUD_FaderReader.h"

AUD_FaderFactory::AUD_FaderFactory(boost::shared_ptr<AUD_IFactory> factory, AUD_FadeType type,
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

boost::shared_ptr<AUD_IReader> AUD_FaderFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_FaderReader(getReader(), m_type, m_start, m_length));
}
