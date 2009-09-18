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

#include "AUD_DelayFactory.h"
#include "AUD_DelayReader.h"
#include "AUD_Space.h"

AUD_DelayFactory::AUD_DelayFactory(AUD_IFactory* factory, float delay) :
		AUD_EffectFactory(factory),
		m_delay(delay) {}

AUD_DelayFactory::AUD_DelayFactory(float delay) :
		AUD_EffectFactory(0),
		m_delay(delay) {}

float AUD_DelayFactory::getDelay()
{
	return m_delay;
}

void AUD_DelayFactory::setDelay(float delay)
{
	m_delay = delay;
}

AUD_IReader* AUD_DelayFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_DelayReader(reader, m_delay); AUD_NEW("reader")
	}

	return reader;
}
