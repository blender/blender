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

#include "AUD_SinusFactory.h"
#include "AUD_SinusReader.h"
#include "AUD_Space.h"

AUD_SinusFactory::AUD_SinusFactory(double frequency, AUD_SampleRate sampleRate)
{
	m_frequency = frequency;
	m_sampleRate = sampleRate;
}

AUD_IReader* AUD_SinusFactory::createReader()
{
	AUD_IReader* reader = new AUD_SinusReader(m_frequency, m_sampleRate);
	AUD_NEW("reader")
	return reader;
}

double AUD_SinusFactory::getFrequency()
{
	return m_frequency;
}

void AUD_SinusFactory::setFrequency(double frequency)
{
	m_frequency = frequency;
}
