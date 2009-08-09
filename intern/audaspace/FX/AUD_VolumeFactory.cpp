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

#include "AUD_VolumeFactory.h"
#include "AUD_VolumeReader.h"

AUD_VolumeFactory::AUD_VolumeFactory(AUD_IFactory* factory, float volume) :
		AUD_EffectFactory(factory),
		m_volume(volume) {}

AUD_VolumeFactory::AUD_VolumeFactory(float volume) :
		AUD_EffectFactory(0),
		m_volume(volume) {}

float AUD_VolumeFactory::getVolume()
{
	return m_volume;
}

void AUD_VolumeFactory::setVolume(float volume)
{
	m_volume = volume;
}

AUD_IReader* AUD_VolumeFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		reader = new AUD_VolumeReader(reader, m_volume); AUD_NEW("reader")
	}

	return reader;
}
