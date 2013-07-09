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

/** \file audaspace/FX/AUD_LimiterFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_LimiterFactory.h"
#include "AUD_LimiterReader.h"
#include "AUD_Space.h"

AUD_LimiterFactory::AUD_LimiterFactory(boost::shared_ptr<AUD_IFactory> factory,
									   float start, float end) :
		AUD_EffectFactory(factory),
		m_start(start),
		m_end(end)
{
}

float AUD_LimiterFactory::getStart() const
{
	return m_start;
}

float AUD_LimiterFactory::getEnd() const
{
	return m_end;
}

boost::shared_ptr<AUD_IReader> AUD_LimiterFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_LimiterReader(getReader(), m_start, m_end));
}
