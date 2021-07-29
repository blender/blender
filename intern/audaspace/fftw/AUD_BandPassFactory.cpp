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

/** \file audaspace/fftw/AUD_BandPassFactory.cpp
 *  \ingroup audfftw
 */


#include "AUD_BandPassFactory.h"
#include "AUD_BandPassReader.h"

AUD_BandPassFactory::AUD_BandPassFactory(AUD_IFactory* factory, float low,
										 float high) :
		AUD_EffectFactory(factory),
		m_low(low),
		m_high(high) {}

AUD_BandPassFactory::AUD_BandPassFactory(float low, float high) :
		AUD_EffectFactory(0),
		m_low(low),
		m_high(high) {}

float AUD_BandPassFactory::getLow()
{
	return m_low;
}

float AUD_BandPassFactory::getHigh()
{
	return m_high;
}

void AUD_BandPassFactory::setLow(float low)
{
	m_low = low;
}

void AUD_BandPassFactory::setHigh(float high)
{
	m_high = high;
}

AUD_IReader* AUD_BandPassFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_BandPassReader(reader, m_low, m_high);
		AUD_NEW("reader")
	}

	return reader;
}
