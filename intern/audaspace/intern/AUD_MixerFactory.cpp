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

#include "AUD_MixerFactory.h"
#include "AUD_IReader.h"

AUD_IReader* AUD_MixerFactory::getReader()
{
	AUD_IReader* reader;

	// first check for an existing reader
	if(m_reader != 0)
	{
		reader = m_reader;
		m_reader = 0;
		return reader;
	}

	// otherwise create a reader if there is a factory
	if(m_factory != 0)
	{
		reader = m_factory->createReader();
		return reader;
	}

	return 0;
}

AUD_MixerFactory::AUD_MixerFactory(AUD_IReader* reader,
											   AUD_Specs specs)
{
	m_specs = specs;
	m_reader = reader;
	m_factory = 0;
}

AUD_MixerFactory::AUD_MixerFactory(AUD_IFactory* factory,
											   AUD_Specs specs)
{
	m_specs = specs;
	m_reader = 0;
	m_factory = factory;
}

AUD_MixerFactory::AUD_MixerFactory(AUD_Specs specs)
{
	m_specs = specs;
	m_reader = 0;
	m_factory = 0;
}

AUD_MixerFactory::~AUD_MixerFactory()
{
	if(m_reader != 0)
	{
		delete m_reader; AUD_DELETE("reader")
	}
}

AUD_Specs AUD_MixerFactory::getSpecs()
{
	return m_specs;
}

void AUD_MixerFactory::setSpecs(AUD_Specs specs)
{
	m_specs = specs;
}

void AUD_MixerFactory::setReader(AUD_IReader* reader)
{
	if(m_reader != 0)
	{
		delete m_reader; AUD_DELETE("reader")
	}
	m_reader = reader;
}

void AUD_MixerFactory::setFactory(AUD_IFactory* factory)
{
	m_factory = factory;
}

AUD_IFactory* AUD_MixerFactory::getFactory()
{
	return m_factory;
}
