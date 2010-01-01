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

#include "AUD_LinearResampleFactory.h"
#include "AUD_LinearResampleReader.h"

AUD_LinearResampleFactory::AUD_LinearResampleFactory(AUD_IReader* reader,
													 AUD_DeviceSpecs specs) :
		AUD_ResampleFactory(reader, specs) {}

AUD_LinearResampleFactory::AUD_LinearResampleFactory(AUD_IFactory* factory,
													 AUD_DeviceSpecs specs) :
		AUD_ResampleFactory(factory, specs) {}

AUD_LinearResampleFactory::AUD_LinearResampleFactory(AUD_DeviceSpecs specs) :
		AUD_ResampleFactory(specs) {}

AUD_IReader* AUD_LinearResampleFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		if(reader->getSpecs().rate != m_specs.rate)
		{
			reader = new AUD_LinearResampleReader(reader, m_specs.specs);
			AUD_NEW("reader")
		}
	}
	return reader;
}
