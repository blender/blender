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

#include "AUD_SDLMixerFactory.h"
#include "AUD_SDLMixerReader.h"

#include <cstring>

AUD_SDLMixerFactory::AUD_SDLMixerFactory(AUD_IReader* reader, AUD_Specs specs) :
		AUD_MixerFactory(reader, specs) {}

AUD_SDLMixerFactory::AUD_SDLMixerFactory(AUD_IFactory* factory, AUD_Specs specs) :
		AUD_MixerFactory(factory, specs) {}

AUD_SDLMixerFactory::AUD_SDLMixerFactory(AUD_Specs specs) :
		AUD_MixerFactory(specs) {}

AUD_IReader* AUD_SDLMixerFactory::createReader()
{
	AUD_IReader* reader = getReader();

	if(reader != 0)
	{
		AUD_Specs specs = reader->getSpecs();
		if(memcmp(&m_specs, &specs, sizeof(AUD_Specs)) != 0)
		{
			try
			{
				reader = new AUD_SDLMixerReader(reader, m_specs);
				AUD_NEW("reader")
			}
			catch(AUD_Exception e)
			{
				// return 0 in case SDL cannot mix the source
				if(e.error != AUD_ERROR_SDL)
					throw;
				else
					reader = NULL;
			}
		}
	}
	return reader;
}
