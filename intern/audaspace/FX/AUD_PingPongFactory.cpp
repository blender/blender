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

#include "AUD_PingPongFactory.h"
#include "AUD_DoubleReader.h"
#include "AUD_ReverseFactory.h"

AUD_PingPongFactory::AUD_PingPongFactory(AUD_IFactory* factory) :
		AUD_EffectFactory(factory)
{
}

AUD_IReader* AUD_PingPongFactory::createReader() const
{
	AUD_IReader* reader = getReader();
	AUD_IReader* reader2;
	AUD_ReverseFactory factory(m_factory);

	try
	{
		reader2 = factory.createReader();
	}
	catch(AUD_Exception&)
	{
		delete reader;
		throw;
	}

	return new AUD_DoubleReader(reader, reader2);
}
