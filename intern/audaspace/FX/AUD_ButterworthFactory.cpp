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

#include "AUD_ButterworthFactory.h"
#include "AUD_ButterworthReader.h"

AUD_ButterworthFactory::AUD_ButterworthFactory(AUD_IFactory* factory,
											   float frequency) :
		AUD_EffectFactory(factory),
		m_frequency(frequency) {}

AUD_ButterworthFactory::AUD_ButterworthFactory(float frequency) :
		AUD_EffectFactory(0),
		m_frequency(frequency) {}

AUD_IReader* AUD_ButterworthFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_ButterworthReader(reader, m_frequency);
		AUD_NEW("reader")
	}

	return reader;
}
